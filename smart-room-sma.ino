// Blynk Credentials
// #define BLYNK_TEMPLATE_ID "your_template_id"
// #define BLYNK_TEMPLATE_NAME "Smart Room SMA"
// #define BLYNK_AUTH_TOKEN "your_auth_token"

#define BLYNK_TEMPLATE_ID "TMPL6pw-KPHta"
#define BLYNK_TEMPLATE_NAME "Smart Room SMA"
#define BLYNK_AUTH_TOKEN "K7gAemOl-gG9R7AQ5lolYqUTRvgg8C9w"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

#define BLYNK_FIRMWARE_VERSION "0.1.0"
#define BLYNK_PRINT Serial
// #define BLYNK_DEBUG
// #define APP_DEBUG

// WiFi Credentials
// char ssid[] = "your_wifi_ssid";
// char pass[] = "your_wifi_password";
char ssid[] = "MIO";
char pass[] = "GOMEZ123";

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

// Global Variables
float temp, hum;
int relayState = 0;

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
      preferences.begin("wifi", false);
      preferences.putString("ssid", newSsid);
      preferences.putString("pass", newPass);
      preferences.end();

      Serial.println("WiFi credentials saved! Restarting...");
      delay(1000);
      ESP.restart();
    } else {
      Serial.println("Error: SSID or Password is empty. Fill both first!");
      Blynk.virtualWrite(VPIN_STATUS, "WiFi save failed: empty field!");
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

// Update OLED Display
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("== SMART ROOM ==");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 16);
  display.print("Suhu   : ");
  if (isnan(temp)) {
    display.println("-- C (error)");
  } else {
    display.print(temp, 1);
    display.println(" C");
  }

  display.setCursor(0, 28);
  display.print("Lembab : ");
  if (isnan(hum)) {
    display.println("-- % (error)");
  } else {
    display.print(hum, 1);
    display.println(" %");
  }

  display.setCursor(0, 40);
  display.print("Relay  : ");
  display.println(relayState ? "ON" : "OFF");

  display.setCursor(0, 52);
  display.print("Connected to  : ");
  display.println(WiFi.SSID());

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
  preferences.begin("wifi", true);
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

  // Blynk Setup
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(SEND_INTERVAL, monitoring);

  // Sync current SSID to Blynk dashboard
  Blynk.virtualWrite(VPIN_WIFI_SSID, ssid);

  // Initial OLED Update
  updateOLED();
}

void loop() {
  Blynk.run();
  timer.run();
  handleManualButtons();
}