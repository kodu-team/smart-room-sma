/*
  =====================================================================
   SMART ROOM SMA
  =====================================================================
   Fitur:
   - Monitoring suhu & kelembaban (sensor DHT22)
   - Kontrol relay (ON/OFF) via aplikasi Blynk
   - Kontrol relay manual via 2 push button (PB1=ON, PB2=OFF)
   - Tampilan data suhu, kelembaban, status relay, & status WiFi di OLED
   - Integrasi platform Blynk (Blynk IoT / Blynk 2.0)

   Board   : ESP32-C3 Mini (Super Mini)
   Sensor  : DHT22
   Display : OLED SSD1306 128x64 (I2C)
   Relay   : Modul relay 1 channel (aktif HIGH)
  =====================================================================
*/

// ---------------------------------------------------------------------
// 1. KREDENSIAL BLYNK
//    Ambil dari Blynk.Console -> Template -> Device Info
// ---------------------------------------------------------------------
#define BLYNK_TEMPLATE_ID   "TMPLxxxxxxx"
#define BLYNK_TEMPLATE_NAME "Smart Room"
#define BLYNK_AUTH_TOKEN    "ISI_AUTH_TOKEN_ANDA"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <time.h>

// ---------------------------------------------------------------------
// 2. KREDENSIAL WIFI
// ---------------------------------------------------------------------
#define PREF_NAMESPACE "wifi_conf"
#define PREF_KEY_SSID  "ssid"
#define PREF_KEY_PASS  "pass"

const char DEFAULT_SSID[] = "NAMA_WIFI_ANDA";
const char DEFAULT_PASS[] = "PASSWORD_WIFI_ANDA";

char ssid[32];
char pass[64];
Preferences prefs;
String remoteSsid = "";
String remotePass = "";

#define VPIN_WIFI_SSID        V3
#define VPIN_WIFI_PASS        V4
#define VPIN_WIFI_SAVE        V5
#define VPIN_WIFI_STATUS      V6
#define VPIN_SCHEDULE_ON      V7
#define VPIN_SCHEDULE_OFF     V8
#define VPIN_SCHEDULE_ENABLE  V9
#define VPIN_SCHEDULE_STATUS V10

#define TIME_ZONE_OFFSET_SECONDS 25200 // GMT+7, sesuaikan jika perlu

String scheduleOn = "";
String scheduleOff = "";
bool scheduleEnabled = false;

// ---------------------------------------------------------------------
// 3. KONFIGURASI PIN (ESP32-C3 Mini)
//    Sesuaikan jika layout board Anda berbeda.
// ---------------------------------------------------------------------
#define DHTPIN     4      // Pin data DHT22
#define DHTTYPE    DHT22
#define RELAY_PIN  5      // Pin kontrol relay
#define PB1_PIN    6      // Push button manual ON
#define PB2_PIN    7      // Push button manual OFF

#define OLED_SDA   8      // I2C SDA untuk OLED
#define OLED_SCL   9      // I2C SCL untuk OLED

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

#define BUZZER_PIN          10      // Pin buzzer piezo (sesuaikan jika perlu)
#define TEMP_ALARM_THRESHOLD 30.0   // Ambang suhu untuk membunyikan alarm

// Set true jika relay module Anda aktif LOW (kebanyakan modul relay 1-channel murah aktif LOW)
#define RELAY_ACTIVE_LOW false

// ---------------------------------------------------------------------
// 4. VIRTUAL PIN BLYNK
//    V0 = suhu (read only, value display)
//    V1 = kelembaban (read only, value display)
//    V2 = kontrol relay (switch/button)
// ---------------------------------------------------------------------
#define VPIN_TEMP   V0
#define VPIN_HUMID  V1
#define VPIN_RELAY  V2

// ---------------------------------------------------------------------
// Objek global
// ---------------------------------------------------------------------
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BlynkTimer timer;

bool  relayState  = false;
bool  alarmActive  = false;
float currentTemp  = NAN;
float currentHumid = NAN;

const unsigned long BUTTON_DEBOUNCE_MS = 50;

// ---------------------------------------------------------------------
// Helper: set relay fisik sesuai state logis + jenis modul (active low/high)
// ---------------------------------------------------------------------
void setRelay(bool state) {
  relayState = state;
  bool pinLevel = RELAY_ACTIVE_LOW ? !state : state;
  digitalWrite(RELAY_PIN, pinLevel ? HIGH : LOW);

  if (Blynk.connected()) {
    Blynk.virtualWrite(VPIN_RELAY, state ? 1 : 0);
  }
}

void setAlarm(bool state) {
  alarmActive = state;
  digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
}

bool connectToWiFi(const char* targetSsid, const char* targetPass, unsigned long timeoutMs = 15000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(targetSsid, targetPass);

  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WIFI] Terhubung ke '%s'\n", targetSsid);
      return true;
    }
    delay(250);
  }

  Serial.printf("[WIFI] Gagal terhubung '%s' dalam %lums\n", targetSsid, timeoutMs);
  return false;
}

void loadWifiCredentials() {
  prefs.begin(PREF_NAMESPACE, true);
  String storedSsid = prefs.getString(PREF_KEY_SSID, "");
  String storedPass = prefs.getString(PREF_KEY_PASS, "");
  prefs.end();

  if (storedSsid.length() > 0 && storedPass.length() > 0) {
    storedSsid.toCharArray(ssid, sizeof(ssid));
    storedPass.toCharArray(pass, sizeof(pass));
  } else {
    strcpy(ssid, DEFAULT_SSID);
    strcpy(pass, DEFAULT_PASS);
  }

  remoteSsid = ssid;
  remotePass = pass;
}

void setDefaultWifiCredentials() {
  strcpy(ssid, DEFAULT_SSID);
  strcpy(pass, DEFAULT_PASS);
  prefs.begin(PREF_NAMESPACE, false);
  prefs.remove(PREF_KEY_SSID);
  prefs.remove(PREF_KEY_PASS);
  prefs.end();
  remoteSsid = ssid;
  remotePass = pass;
}

bool saveWifiCredentials(const String& newSsid, const String& newPass, String& statusMessage) {
  if (newSsid.length() == 0 || newPass.length() == 0) {
    Serial.println("[WIFI] SSID atau password kosong, tidak disimpan.");
    statusMessage = "SSID/Pass kosong";
    return false;
  }

  char candidateSsid[32];
  char candidatePass[64];
  newSsid.toCharArray(candidateSsid, sizeof(candidateSsid));
  newPass.toCharArray(candidatePass, sizeof(candidatePass));

  if (!connectToWiFi(candidateSsid, candidatePass, 15000)) {
    Serial.println("[WIFI] Koneksi baru gagal, coba kembali ke default.");
    setDefaultWifiCredentials();
    if (connectToWiFi(ssid, pass, 15000)) {
      Serial.println("[WIFI] Berhasil kembali ke default.");
      statusMessage = "Gagal, default aktif";
      return true;
    }
    Serial.println("[WIFI] Gagal terhubung ke default juga.");
    statusMessage = "Gagal konek default";
    return false;
  }

  strcpy(ssid, candidateSsid);
  strcpy(pass, candidatePass);
  prefs.begin(PREF_NAMESPACE, false);
  prefs.putString(PREF_KEY_SSID, newSsid);
  prefs.putString(PREF_KEY_PASS, newPass);
  prefs.end();

  remoteSsid = ssid;
  remotePass = pass;
  statusMessage = "Tersimpan & Terhubung";
  return true;
}

bool isValidTimeString(const String& value) {
  if (value.length() != 5 || value.charAt(2) != ':') {
    return false;
  }

  int hour = value.substring(0, 2).toInt();
  int minute = value.substring(3, 5).toInt();
  return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}

void loadSchedule() {
  prefs.begin(PREF_NAMESPACE, true);
  scheduleOn = prefs.getString("sched_on", "");
  scheduleOff = prefs.getString("sched_off", "");
  scheduleEnabled = prefs.getBool("sched_enabled", false);
  prefs.end();
}

void saveSchedulePreference(const char* key, const String& value) {
  prefs.begin(PREF_NAMESPACE, false);
  if (value.length() == 0) {
    prefs.remove(key);
  } else {
    prefs.putString(key, value);
  }
  prefs.end();
}

void saveScheduleEnabled(bool enabled) {
  prefs.begin(PREF_NAMESPACE, false);
  prefs.putBool("sched_enabled", enabled);
  prefs.end();
}

void updateScheduleStatus() {
  String status = scheduleEnabled ? "Aktif" : "Nonaktif";
  if (scheduleOn.length() > 0) {
    status += " On:" + scheduleOn;
  }
  if (scheduleOff.length() > 0) {
    status += " Off:" + scheduleOff;
  }
  if (!scheduleEnabled && scheduleOn.length() == 0 && scheduleOff.length() == 0) {
    status = "Jadwal nonaktif";
  }
  if (Blynk.connected()) {
    Blynk.virtualWrite(VPIN_SCHEDULE_STATUS, status);
  }
}

void checkSchedule() {
  if (!scheduleEnabled) {
    return;
  }

  struct tm timeInfo;
  if (!getLocalTime(&timeInfo)) {
    Serial.println("[TIME] Gagal mendapatkan waktu NTP.");
    return;
  }

  static int lastMinute = -1;
  if (timeInfo.tm_min == lastMinute) {
    return;
  }
  lastMinute = timeInfo.tm_min;

  char currentTime[6];
  sprintf(currentTime, "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
  String now = currentTime;

  if (scheduleOn.length() > 0 && now == scheduleOn) {
    Serial.printf("[SCHEDULE] %s cocok jadwal ON, menyalakan relay.\n", currentTime);
    setRelay(true);
  }

  if (scheduleOff.length() > 0 && now == scheduleOff) {
    Serial.printf("[SCHEDULE] %s cocok jadwal OFF, mematikan relay.\n", currentTime);
    setRelay(false);
  }
}

// ---------------------------------------------------------------------
// Tampilkan data terbaru ke OLED
// ---------------------------------------------------------------------
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("== SMART ROOM ==");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 16);
  display.print("Suhu   : ");
  if (isnan(currentTemp)) {
    display.println("-- C (error)");
  } else {
    display.print(currentTemp, 1);
    display.println(" C");
  }

  display.setCursor(0, 28);
  display.print("Lembab : ");
  if (isnan(currentHumid)) {
    display.println("-- % (error)");
  } else {
    display.print(currentHumid, 1);
    display.println(" %");
  }

  display.setCursor(0, 42);
  display.print("Relay  : ");
  display.println(relayState ? "ON" : "OFF");

  display.setCursor(0, 54);
  display.print("Alarm  : ");
  display.println(alarmActive ? "PANAS" : "OK");

  display.setCursor(0, 62);
  display.print("WiFi   : ");
  display.println(WiFi.status() == WL_CONNECTED ? "Terhubung" : "Terputus");

  display.display();
}

// ---------------------------------------------------------------------
// Kontrol relay manual lewat 2 push button
// ---------------------------------------------------------------------
void handleManualButtons() {
  static bool lastPb1State = HIGH;
  static bool lastPb2State = HIGH;
  static unsigned long lastDebounceTime = 0;

  unsigned long now = millis();
  if (now - lastDebounceTime < BUTTON_DEBOUNCE_MS) {
    return;
  }
  lastDebounceTime = now;

  bool pb1State = digitalRead(PB1_PIN);
  bool pb2State = digitalRead(PB2_PIN);

  if (pb1State != lastPb1State) {
    lastPb1State = pb1State;
    if (pb1State == LOW) {
      setRelay(true);
      Serial.println("[MANUAL] Relay ON via PB1");
      updateOLED();
    }
  }

  if (pb2State != lastPb2State) {
    lastPb2State = pb2State;
    if (pb2State == LOW) {
      setRelay(false);
      Serial.println("[MANUAL] Relay OFF via PB2");
      updateOLED();
    }
  }
}

// ---------------------------------------------------------------------
// Baca sensor DHT22 lalu kirim ke Blynk & OLED
// ---------------------------------------------------------------------
void readSensorAndSend() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("[DHT22] Gagal membaca sensor!");
    currentTemp  = NAN;
    currentHumid = NAN;
    updateOLED();
    return;
  }

  currentTemp  = t;
  currentHumid = h;

  Serial.printf("[DHT22] Suhu: %.1f C | Kelembaban: %.1f %%\n", t, h);

  Blynk.virtualWrite(VPIN_TEMP, t);
  Blynk.virtualWrite(VPIN_HUMID, h);

  bool overThreshold = currentTemp > TEMP_ALARM_THRESHOLD;
  bool wasAlarmActive = alarmActive;
  setAlarm(overThreshold);

  if (overThreshold && !wasAlarmActive) {
    Serial.printf("[ALARM] Suhu melewati %.1f C!\n", TEMP_ALARM_THRESHOLD);
    if (Blynk.connected()) {
      Blynk.notify("Alarm: suhu di atas threshold! Cek ruangan segera.");
    }
  }

  updateOLED();
}

// ---------------------------------------------------------------------
// Dipanggil otomatis saat tombol/switch relay di app Blynk ditekan
// ---------------------------------------------------------------------
BLYNK_WRITE(VPIN_RELAY) {
  int value = param.asInt(); // 0 atau 1
  setRelay(value == 1);
  Serial.printf("[RELAY] Diset ke %s dari Blynk\n", relayState ? "ON" : "OFF");
  updateOLED();
}

BLYNK_WRITE(VPIN_WIFI_SSID) {
  remoteSsid = param.asStr();
  Serial.printf("[BLYNK] WiFi SSID diubah menjadi '%s'\n", remoteSsid.c_str());
}

BLYNK_WRITE(VPIN_WIFI_PASS) {
  remotePass = param.asStr();
  Serial.println("[BLYNK] WiFi password diterima");
}

BLYNK_WRITE(VPIN_WIFI_SAVE) {
  int value = param.asInt();
  if (value == 1) {
    Serial.println("[BLYNK] Simpan konfigurasi WiFi diterima");
    String statusMessage;
    bool ok = saveWifiCredentials(remoteSsid, remotePass, statusMessage);
    Blynk.virtualWrite(VPIN_WIFI_STATUS, statusMessage);
    if (!ok && WiFi.status() != WL_CONNECTED) {
      Blynk.virtualWrite(VPIN_WIFI_STATUS, "Gagal, default aktif");
    }
    Blynk.virtualWrite(VPIN_WIFI_SSID, ssid);
    Blynk.virtualWrite(VPIN_WIFI_PASS, pass);
    updateOLED();
  }
}

BLYNK_WRITE(VPIN_SCHEDULE_ON) {
  String value = param.asStr();
  if (value.length() == 0) {
    scheduleOn = "";
    saveSchedulePreference("sched_on", scheduleOn);
    Serial.println("[BLYNK] Jadwal ON dihapus");
  } else if (isValidTimeString(value)) {
    scheduleOn = value;
    saveSchedulePreference("sched_on", scheduleOn);
    Serial.printf("[BLYNK] Jadwal ON disimpan %s\n", scheduleOn.c_str());
  } else {
    Serial.println("[BLYNK] Format jadwal ON tidak valid, gunakan hh:mm");
  }
  updateScheduleStatus();
}

BLYNK_WRITE(VPIN_SCHEDULE_OFF) {
  String value = param.asStr();
  if (value.length() == 0) {
    scheduleOff = "";
    saveSchedulePreference("sched_off", scheduleOff);
    Serial.println("[BLYNK] Jadwal OFF dihapus");
  } else if (isValidTimeString(value)) {
    scheduleOff = value;
    saveSchedulePreference("sched_off", scheduleOff);
    Serial.printf("[BLYNK] Jadwal OFF disimpan %s\n", scheduleOff.c_str());
  } else {
    Serial.println("[BLYNK] Format jadwal OFF tidak valid, gunakan hh:mm");
  }
  updateScheduleStatus();
}

BLYNK_WRITE(VPIN_SCHEDULE_ENABLE) {
  scheduleEnabled = param.asInt() == 1;
  saveScheduleEnabled(scheduleEnabled);
  Serial.printf("[BLYNK] Jadwal %s\n", scheduleEnabled ? "diaktifkan" : "dinonaktifkan");
  updateScheduleStatus();
}

// ---------------------------------------------------------------------
// Sinkronisasi state setiap kali berhasil connect/reconnect ke Blynk
// ---------------------------------------------------------------------
BLYNK_CONNECTED() {
  Blynk.syncVirtual(VPIN_RELAY);
  Blynk.virtualWrite(VPIN_WIFI_SSID, ssid);
  Blynk.virtualWrite(VPIN_WIFI_PASS, pass);
  Blynk.virtualWrite(VPIN_WIFI_STATUS, WiFi.status() == WL_CONNECTED ? "Terhubung" : "Terputus");
  Blynk.virtualWrite(VPIN_SCHEDULE_ON, scheduleOn);
  Blynk.virtualWrite(VPIN_SCHEDULE_OFF, scheduleOff);
  Blynk.virtualWrite(VPIN_SCHEDULE_ENABLE, scheduleEnabled ? 1 : 0);
  updateScheduleStatus();
}

// ---------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PB1_PIN, INPUT_PULLUP);
  pinMode(PB2_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  setRelay(false); // pastikan relay OFF saat boot
  setAlarm(false); // pastikan alarm mati saat boot

  dht.begin();

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("[OLED] Tidak terdeteksi, cek wiring I2C!");
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println("Memuat WiFi... ");
  display.display();

  loadWifiCredentials();
  loadSchedule();
  bool wifiOk = connectToWiFi(ssid, pass, 15000);
  if (!wifiOk && (strcmp(ssid, DEFAULT_SSID) != 0 || strcmp(pass, DEFAULT_PASS) != 0)) {
    Serial.println("[WIFI] Koneksi gagal dengan kredensial tersimpan, kembali ke default.");
    setDefaultWifiCredentials();
    wifiOk = connectToWiFi(ssid, pass, 15000);
  }

  if (wifiOk) {
    configTime(TIME_ZONE_OFFSET_SECONDS, 0, "pool.ntp.org", "time.nist.gov");
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  } else {
    Serial.println("[WIFI] Tidak bisa terhubung ke WiFi, Blynk tidak dimulai.");
  }

  // Baca & kirim data sensor setiap 2 detik
  timer.setInterval(2000L, readSensorAndSend);
  timer.setInterval(60000L, checkSchedule);
  checkSchedule();

  updateOLED();
}

// ---------------------------------------------------------------------
// LOOP
// ---------------------------------------------------------------------
void loop() {
  handleManualButtons();
  Blynk.run();
  timer.run();
}
