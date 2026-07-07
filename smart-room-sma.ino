// Blynk Credentials
#define BLYNK_TEMPLATE_ID "your_template_id"
#define BLYNK_TEMPLATE_NAME "Smart Room SMA"
#define BLYNK_AUTH_TOKEN "your_auth_token"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <time.h>

#define BLYNK_FIRMWARE_VERSION "0.1.0"
#define BLYNK_PRINT Serial
// #define BLYNK_DEBUG
// #define APP_DEBUG

// WiFi Credentials
char ssid[] = "your_wifi_ssid";
char pass[] = "your_wifi_password";

// Temporary storage for new WiFi credentials from Blynk
String newSsid = "";
String newPass = "";

// Preferences untuk menyimpan WiFi credentials
Preferences preferences;

// Pinout & Constants
#define DHT_PIN 4
#define RELAY_PIN 5
#define DHTTYPE DHT22
#define HIGH_TEMP 35
#define PB1_PIN 6
#define PB2_PIN 7
#define OLED_SDA 8
#define OLED_SCL 9
#define SEND_INTERVAL 5000L
#define BUZZER_PIN 10

// Virtual Pin Blynk
#define VPIN_TEMP V0 // Temperature
#define VPIN_HUMIDITY V1 // Humidity
#define VPIN_RELAY V2 // Relay Control
#define VPIN_STATUS V3 // Status
#define VPIN_WIFI_SSID V4 // WiFi SSID Input
#define VPIN_WIFI_PASS V5 // WiFi Password Input
#define VPIN_WIFI_SAVE V6 // WiFi Save Trigger
#define VPIN_SCH_ENABLE V7  // Schedule Enable/Disable
#define VPIN_SCH_ON V8      // Schedule ON Time (HH:MM)
#define VPIN_SCH_OFF V9     // Schedule OFF Time (HH:MM)
#define VPIN_SCH_UPDATE V10 // Schedule Update Trigger

// Global Variables
float temp, hum;
int relayState = 0;

// Schedule Variables
int scheduleEnabled = 0;
String scheduleOnTime = "00:00";
String scheduleOffTime = "00:00";
bool scheduleActionDone = false;
char currentTime[6];
char lastCheckedMinute[6] = "";

// NTP Time
struct tm timeinfo;

// Class Declaration
BlynkTimer timer;
DHT dht(DHT_PIN, DHTTYPE);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// WiFi SSID Input dari Blynk Dashboard (V4)
BLYNK_WRITE(VPIN_WIFI_SSID) {
  newSsid = param.asStr();
  Serial.println("New WiFi SSID received: " + newSsid);
}

// WiFi Password Input dari Blynk Dashboard (V5)
BLYNK_WRITE(VPIN_WIFI_PASS) {
  newPass = param.asStr();
  Serial.println("New WiFi Password received: " + newPass);
}

// Save Trigger dari Blynk Dashboard (V6)
BLYNK_WRITE(VPIN_WIFI_SAVE) {
  int trigger = param.asInt();

  if (trigger == 1) {
    if (newSsid.length() > 0 && newPass.length() > 0) {
      Serial.println("Saving new WiFi credentials to Preferences...");
      Serial.println("SSID: " + newSsid);

      // Simpan ke Preferences
      preferences.begin("config", false);
      preferences.putString("ssid", newSsid);
      preferences.putString("pass", newPass);
      preferences.end();

      Serial.println("WiFi credentials saved! Restarting...");
      delay(1000);
      ESP.restart();
    } else {
      Serial.println("Error: SSID or Password is empty. Fill both first!");
    }
  }
}

// Controlling Relay
BLYNK_WRITE(VPIN_RELAY) {
  relayState = param.asInt();

  if (relayState == 1) {
    Serial.println("Relay ON via Blynk");
    digitalWrite(RELAY_PIN, HIGH);
    Serial.print("relayState = ");
    Serial.println(relayState);
  } else {
    Serial.println("Relay OFF via Blynk");
    digitalWrite(RELAY_PIN, LOW);
    Serial.print("relayState = ");
    Serial.println(relayState);
  }

  // Update OLED
  updateOLED();
}

// Schedule Enable/Disable Switch (V7)
BLYNK_WRITE(VPIN_SCH_ENABLE) {
  scheduleEnabled = param.asInt();
  Serial.print("Schedule mode: ");
  Serial.println(scheduleEnabled? "ENABLED" : "DISABLED");

  preferences.begin("config", false);
  preferences.putInt("sch_enable", scheduleEnabled);
  preferences.end();

  Blynk.virtualWrite(VPIN_SCH_ENABLE, scheduleEnabled);

  updateOLED();
}

// Schedule ON Time Input (V8)
BLYNK_WRITE(VPIN_SCH_ON) {
  scheduleOnTime = param.asStr();
  Serial.println("Schedule ON Time: " + scheduleOnTime);
}

// Schedule OFF Time Input (V9)
BLYNK_WRITE(VPIN_SCH_OFF) {
  scheduleOffTime = param.asStr();
  Serial.println("Schedule OFF Time: " + scheduleOffTime);
}

// Schedule Update Trigger Button (V10)
BLYNK_WRITE(VPIN_SCH_UPDATE) {
  int trigger = param.asInt();
  if (trigger == 1) {
    Serial.println("Saving schedule to Preferences...");
    Serial.println("ON: " + scheduleOnTime + ", OFF: " + scheduleOffTime);

    preferences.begin("config", false);
    preferences.putString("sch_on", scheduleOnTime);
    preferences.putString("sch_off", scheduleOffTime);
    preferences.end();

    Serial.println("Schedule saved!");
    scheduleActionDone = false;
  }
}

// Load schedule from Preferences
void loadSchedule() {
  preferences.begin("config", true);
  scheduleEnabled = preferences.getInt("sch_enable", 0);
  scheduleOnTime = preferences.getString("sch_on", "00:00");
  scheduleOffTime = preferences.getString("sch_off", "00:00");
  preferences.end();

  Serial.println("Schedule loaded from Preferences:");
  Serial.print("  Mode: "); Serial.println(scheduleEnabled? "ENABLED" : "DISABLED");
  Serial.print("  ON Time: "); Serial.println(scheduleOnTime);
  Serial.print("  OFF Time: "); Serial.println(scheduleOffTime);

  // Sync to Blynk dashboard
  Blynk.virtualWrite(VPIN_SCH_ENABLE, scheduleEnabled);
  Blynk.virtualWrite(VPIN_SCH_ON, scheduleOnTime);
  Blynk.virtualWrite(VPIN_SCH_OFF, scheduleOffTime);
}

// Check schedule every minute
void checkSchedule() {
  if (!scheduleEnabled) {
    return;
  }

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain NTP time");
    return;
  }

  snprintf(currentTime, sizeof(currentTime), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  // Only check once per minute
  if (strcmp(currentTime, lastCheckedMinute) == 0) {
    return;
  }
  strncpy(lastCheckedMinute, currentTime, sizeof(lastCheckedMinute));

  Serial.print("Schedule check at: ");
  Serial.println(currentTime);

  if (strcmp(currentTime, scheduleOnTime.c_str()) == 0) {
    Serial.println("Schedule: ON time matched! Turning relay ON.");
    digitalWrite(RELAY_PIN, HIGH);
    relayState = 1;
    Blynk.virtualWrite(VPIN_RELAY, relayState);
  }
  else if (strcmp(currentTime, scheduleOffTime.c_str()) == 0) {
    Serial.println("Schedule: OFF time matched! Turning relay OFF.");
    digitalWrite(RELAY_PIN, LOW);
    relayState = 0;
    Blynk.virtualWrite(VPIN_RELAY, relayState);
  }
  
  updateOLED();
}

// Update OLED Display
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("== SMART ROOM ==");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 16);
  if (isnan(temp)) {
    display.print("-- C | ");
  } else {
    display.print(temp, 1);
    display.print(" C | ");
  }

  if (isnan(hum)) {
    display.print("-- % | ");
  } else {
    display.print(hum, 1);
    display.print(" % | ");
  }
  
  display.println(relayState ? "ON" : "OFF");
  
  display.setCursor(0, 28);
  display.print("WiFi : ");
  display.println(WiFi.SSID());
  
  if (scheduleEnabled) {
    display.setCursor(0, 40);
    display.print("Time : ");
    display.println(String(currentTime));
  
    display.setCursor(0, 52);
    display.print("ON:");
    display.print(scheduleOnTime);
    display.print(" | ");
    display.print("OFF:");
    display.println(scheduleOffTime);
  }

  display.display();
}

void handleManualButtons() {
  static bool lastPb1State = HIGH;
  static bool lastPb2State = HIGH;
  static unsigned long lastDebounceTime = 0;

  unsigned long now = millis();
  if (now - lastDebounceTime < 50) {
    return;
  }
  lastDebounceTime = now;

  bool pb1State = digitalRead(PB1_PIN);
  bool pb2State = digitalRead(PB2_PIN);

  if (pb1State != lastPb1State) {
    lastPb1State = pb1State;
    if (pb1State == LOW) {
      Serial.println("Relay ON via PB1");
      digitalWrite(RELAY_PIN, HIGH);
      relayState = 1;

      // Sync with Blynk app
      Blynk.virtualWrite(VPIN_RELAY, relayState);

      // Update OLED
      updateOLED();
    }
  }

  if (pb2State != lastPb2State) {
    lastPb2State = pb2State;
    if (pb2State == LOW) {
      Serial.println("Relay OFF via PB2");
      digitalWrite(RELAY_PIN, LOW);
      relayState = 0;
      
      // Sync with Blynk app
      Blynk.virtualWrite(VPIN_RELAY, relayState);

      // Update OLED
      updateOLED();
    }
  }
}

// Monitoring Temperature & Humidity
void monitoring() {
  // Read Sensor
  temp = dht.readTemperature();
  hum = dht.readHumidity();
  if (isnan(hum) || isnan(temp)) {
    Serial.println("Failed to read DHT sensor!");
    return;
  }

  // Debug
  Serial.println("Temperature = " + String(temp) + " C");
  Serial.println("Humidity = " + String(hum) + " %");

  // Send Data
  Blynk.virtualWrite(VPIN_TEMP, temp);
  Blynk.virtualWrite(VPIN_HUMIDITY, hum);

  // Alarm & Notification
  if (temp > HIGH_TEMP) {
    Serial.println("High temperature detected!");
    Blynk.logEvent("high_temp", "Temperature exceeded threshold: " + String(temp) + " C");
    Blynk.virtualWrite(VPIN_STATUS, "HIGH TEMP!");
    
    // Turn on buzzer
    digitalWrite(BUZZER_PIN, HIGH);
  }
  else {
    Blynk.virtualWrite(VPIN_STATUS, "NORMAL");

    // Turn off buzzer
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Update OLED
  updateOLED();
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Push Button Setup
  pinMode(PB1_PIN, INPUT_PULLUP);
  pinMode(PB2_PIN, INPUT_PULLUP);

  // Relay Setup
  pinMode(RELAY_PIN, OUTPUT);

  // Buzzer Setup
  pinMode(BUZZER_PIN, OUTPUT);

  // DHT Setup
  dht.begin();

  // OLED Setup
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED tidak terdeteksi, cek wiring I2C!");
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println("Menghubungkan WiFi...");
  display.display();

  // Baca WiFi credentials dari Preferences (NVS)
  preferences.begin("config", true);
  String savedSsid = preferences.getString("ssid", "");
  String savedPass = preferences.getString("pass", "");
  preferences.end();

  if (savedSsid.length() > 0 && savedPass.length() > 0) {
    // Gunakan kredensial tersimpan dari Preferences
    Serial.println("Using WiFi credentials from Preferences.");
    Serial.println("SSID: " + savedSsid);
    savedSsid.toCharArray(ssid, sizeof(ssid));
    savedPass.toCharArray(pass, sizeof(pass));

    // Sync nilai ke string sementara
    newSsid = savedSsid;
    newPass = savedPass;
  } else {
    Serial.println("No saved WiFi credentials. Using hardcoded fallback.");
  }

  // Init NTP time sync (WIB = UTC+7)
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for NTP time sync...");
  int ntpRetry = 0;
  while (!getLocalTime(&timeinfo) && ntpRetry < 20) {
    delay(500);
    Serial.print(".");
    ntpRetry++;
  }
  if (getLocalTime(&timeinfo)) {
    Serial.println("\nNTP time synced!");
    Serial.print("Current time: ");
    Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
  } else {
    Serial.println("\nFailed to sync NTP time, will retry later.");
  }

  // Blynk Setup
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(SEND_INTERVAL, monitoring);

  // Sync current SSID to Blynk dashboard
  Blynk.virtualWrite(VPIN_WIFI_SSID, ssid);

  // Load schedule from Preferences and sync to dashboard
  loadSchedule();

  // Register schedule checker every 60 seconds
  timer.setInterval(60000L, checkSchedule);

  // Initial OLED Update
  updateOLED();
}

void loop() {
  Blynk.run();
  timer.run();
  handleManualButtons();
}