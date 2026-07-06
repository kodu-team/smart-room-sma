# Smart Room SMA

Sistem monitoring & kontrol ruangan berbasis IoT menggunakan **ESP32-C3 Mini**, sensor suhu **DHT22**, **modul relay**, layar **OLED**, dan platform **Blynk**.

## Fitur

- 🌡️ Monitoring suhu & kelembaban real-time (DHT22)
- 🔌 Kontrol relay ON/OFF dari aplikasi Blynk (jarak jauh)
- 🖲️ Kontrol relay manual via 2 push button (PB1 = ON, PB2 = OFF)
- 🔔 Alarm buzzer otomatis saat suhu melebihi threshold dan notifikasi ke Blynk
- 📟 Tampilan suhu, kelembaban, status relay, status alarm, dan status WiFi di layar OLED
- 🌐 Remote WiFi configuration via Blynk dashboard dengan penyimpanan Preferensi
- ⏰ Jadwal relay ON/OFF tersimpan dengan switch aktif/nonaktif di Blynk
- ☁️ Sinkronisasi data ke dashboard Blynk (grafik & histori otomatis tersedia di Blynk)
- 🔄 Auto-sync status relay saat board reconnect ke Blynk (state tidak hilang)

## Komponen yang Dibutuhkan

| Komponen | Jumlah | Keterangan |
| --- | --- | --- |
| ESP32-C3 Mini (Super Mini) | 1 | Mikrokontroler utama, WiFi built-in |
| Sensor DHT22 | 1 | Sensor suhu & kelembaban |
| Modul Relay 1 Channel | 1 | Untuk switching beban AC/DC (lampu, kipas, dll) |
| OLED SSD1306 128x64 (I2C) | 1 | Display data |
| Push Button | 2 | Untuk kontrol manual relay (PB1 ON, PB2 OFF) |
| Buzzer Piezo | 1 | Alarm suhu otomatis saat threshold terlampaui |
| Resistor 10kΩ | 1 (opsional) | Pull-up untuk DHT22 jika modul tidak sudah include |
| Kabel jumper + breadboard | secukupnya | |
| Adaptor 5V / kabel USB-C | 1 | Power supply |

## Skema Wiring

| Perangkat | Pin Perangkat | Pin ESP32-C3 Mini |
| --- | --- | --- |
| DHT22 | VCC | 3V3 |
| DHT22 | GND | GND |
| DHT22 | DATA | GPIO 4 |
| Relay | VCC | 5V / VIN |
| Relay | GND | GND |
| Relay | IN | GPIO 5 |
| Push Button PB1 | satu kaki | GPIO 6 |
| Push Button PB1 | kaki lain | GND |
| Push Button PB2 | satu kaki | GPIO 7 |
| Push Button PB2 | kaki lain | GND |
| OLED | VCC | 3V3 |
| OLED | GND | GND |
| OLED | SDA | GPIO 8 |
| OLED | SCL | GPIO 9 |
| Buzzer Piezo | + | GPIO 10 |
| Buzzer Piezo | - | GND |

> ⚠️ Beberapa varian board ESP32-C3 Mini punya pin default I2C atau strapping pin berbeda. Jika GPIO 8/9 dipakai board Anda untuk fungsi lain (mis. boot mode), sesuaikan `OLED_SDA` / `OLED_SCL` di kode dan pastikan tidak bentrok saat upload.
> Tombol manual memakai `INPUT_PULLUP`, jadi saat tombol ditekan pin akan menjadi LOW. Jika Anda menggunakan skema berbeda, sesuaikan pin dan logika di kode.
> Untuk relay yang modulnya **aktif LOW** (kebanyakan modul relay 1-channel murah), ubah `RELAY_ACTIVE_LOW` menjadi `true` di kode.

## Instalasi Arduino IDE

### 1. Tambahkan Board Manager ESP32

`File > Preferences > Additional Board Manager URLs`, isi dengan:

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Lalu buka `Tools > Board > Board Manager`, cari **esp32 by Espressif Systems**, install.

Pilih board: **ESP32C3 Dev Module** (atau varian sesuai board Anda, mis. "Super Mini ESP32C3").

### 2. Install Library (via Library Manager)

- **Blynk** (by Volodymyr Shymanskyy)
- **DHT sensor library** (by Adafruit)
- **Adafruit Unified Sensor** (dependency DHT library)
- **Adafruit GFX Library**
- **Adafruit SSD1306**

## Setup Project di Blynk

1. Buat akun di [Blynk.Console](https://blynk.cloud).
2. Buat **Template** baru, catat **Template ID** dan **Template Name**.
3. Tambahkan datastream berikut:

   | Nama | Virtual Pin | Tipe Data | Keterangan |
   | --- | --- | --- | --- |
   | Suhu | V0 | Double, satuan °C | Read only dari device |
   | Kelembaban | V1 | Double, satuan % | Read only dari device |
   | Relay | V2 | Integer (0/1) | Switch, bisa ditulis dari app |
   | WiFi SSID | V3 | String | Input SSID WiFi baru |
   | WiFi Password | V4 | String | Input password WiFi baru |
   | Save WiFi | V5 | Integer (0/1) | Tombol save WiFi baru |
   | WiFi Status | V6 | String | Status koneksi / simpan |
   | Schedule ON | V7 | String | Input waktu nyala hh:mm |
   | Schedule OFF | V8 | String | Input waktu mati hh:mm |
   | Schedule Enable | V9 | Integer (0/1) | Switch aktif/nonaktif jadwal |
   | Schedule Status | V10 | String | Status jadwal saat ini |

4. Buat **Device** baru dari template tersebut, salin **Auth Token**.
5. Di aplikasi Blynk (mobile), buat dashboard dengan widget:
   - **Gauge/Value Display** → bind ke V0 (Suhu)
   - **Gauge/Value Display** → bind ke V1 (Kelembaban)
   - **Switch/Button** → bind ke V2 (Relay)
   - **Text Input** → bind ke V3 (WiFi SSID)
   - **Text Input** → bind ke V4 (WiFi Password)
   - **Button** → bind ke V5 (Save WiFi)
   - **Value Display** → bind ke V6 (WiFi Status)
   - **Text Input** → bind ke V7 (Schedule ON hh:mm)
   - **Text Input** → bind ke V8 (Schedule OFF hh:mm)
   - **Switch/Button** → bind ke V9 (Schedule Enable)
   - **Value Display** → bind ke V10 (Schedule Status)

## Konfigurasi Kode

Sebelum upload, ubah bagian berikut di `smart-room-sma.ino`:

```cpp
#define BLYNK_TEMPLATE_ID   "TMPLxxxxxxx"     // dari Blynk.Console
#define BLYNK_TEMPLATE_NAME "Smart Room"
#define BLYNK_AUTH_TOKEN    "ISI_AUTH_TOKEN_ANDA"

const char DEFAULT_SSID[] = "NAMA_WIFI_ANDA";
const char DEFAULT_PASS[] = "PASSWORD_WIFI_ANDA";
```

Jika ingin mengubah ambang alarm, sesuaikan nilai `TEMP_ALARM_THRESHOLD` di awal kode. Pastikan `BUZZER_PIN` juga cocok dengan koneksi hardware Anda.

## Cara Upload

1. Hubungkan ESP32-C3 Mini ke komputer via USB.
2. Pilih **Board**: ESP32C3 Dev Module, **Port** sesuai COM/tty yang muncul.
3. Klik **Upload**.
4. Buka **Serial Monitor** (baud rate `115200`) untuk melihat log koneksi WiFi/Blynk dan pembacaan sensor.
5. OLED akan menampilkan status WiFi, suhu, kelembaban, dan status relay secara berkala (update setiap 2 detik).
6. Jika tombol manual dipakai, PB1 akan mengaktifkan relay dan PB2 akan mematikannya, tanpa mengganggu kontrol dari aplikasi Blynk.
