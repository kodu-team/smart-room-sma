# Smart Room SMA

Sistem monitoring & kontrol ruangan berbasis IoT menggunakan **ESP32-C3 Mini**, sensor suhu **DHT22**, **modul relay**, layar **OLED**, dan platform **Blynk**.

## Fitur

- 🌡️ Monitoring suhu & kelembaban real-time (sensor DHT22)
- 🔌 Kontrol relay ON/OFF dari aplikasi Blynk (jarak jauh)
- 🖲️ Kontrol relay manual via 2 push button (PB1 = ON, PB2 = OFF) dengan debounce 50ms
- 🔔 Alarm buzzer otomatis saat suhu melebihi ambang batas dan event notifikasi ke Blynk
- 📟 Tampilan suhu, kelembaban, status relay, SSID WiFi, dan jadwal di layar OLED
- ☁️ Sinkronisasi data ke dashboard Blynk melalui virtual pin V0-V10
- 🔄 Sinkronisasi dua arah: tombol manual memperbarui status di Blynk, perintah Blynk memperbarui relay
- 📡 WiFi credentials dapat diubah dari dashboard Blynk tanpa perlu upload ulang kode
- 🕒 Fitur penjadwalan relay otomatis berdasarkan waktu (ON/OFF time schedule)
- ⏰ Sinkronisasi waktu NTP (WIB / UTC+7)
- 💾 Penyimpanan persisten WiFi & jadwal menggunakan ESP32 Preferences (NVS)

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
| DHT22 | DATA | GPIO 3 |
| Relay | VCC | 5V / VIN |
| Relay | GND | GND |
| Relay | IN | GPIO 7 |
| Push Button PB1 | satu kaki | GPIO 0 |
| Push Button PB1 | kaki lain | GND |
| Push Button PB2 | satu kaki | GPIO 1 |
| Push Button PB2 | kaki lain | GND |
| OLED | VCC | 3V3 |
| OLED | GND | GND |
| OLED | SDA | GPIO 8 |
| OLED | SCL | GPIO 9 |
| Buzzer Piezo | + | GPIO 6 |
| Buzzer Piezo | - | GND |

> ⚠️ Beberapa varian board ESP32-C3 Mini punya pin default I2C atau strapping pin berbeda. Jika GPIO 8/9 dipakai board Anda untuk fungsi lain (mis. boot mode), sesuaikan `OLED_SDA` / `OLED_SCL` di kode dan pastikan tidak bentrok saat upload.
> Tombol manual memakai `INPUT_PULLUP`, jadi saat tombol ditekan pin akan menjadi LOW. Jika Anda menggunakan skema berbeda, sesuaikan pin dan logika di kode.
> **Default relay adalah active LOW** (`relayActiveLow = true` di kode). Jika modul relay Anda active HIGH, ubah variabel `relayActiveLow` menjadi `false` di kode.

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

> Library `WiFi`, `WiFiClient`, `Wire`, `Preferences`, dan `time` sudah include di board package ESP32, tidak perlu install terpisah.

## Setup Project di Blynk

1. Buat akun di [Blynk.Console](https://blynk.cloud).
2. Buat **Template** baru, catat **Template ID** dan **Template Name**.
3. Tambahkan 11 **Datastream**:

   | Nama | Virtual Pin | Tipe Data | Keterangan |
   | --- | --- | --- | --- |
   | Suhu | V0 | Double, satuan °C | Read only dari device |
   | Kelembaban | V1 | Double, satuan % | Read only dari device |
   | Relay | V2 | Integer (0/1) | Switch, bisa ditulis dari app & dibaca |
   | Status | V3 | String | Menampilkan status `NORMAL` atau `HIGH TEMP!` |
   | WiFi SSID | V4 | String | Input SSID WiFi baru dari dashboard |
   | WiFi Password | V5 | String | Input password WiFi baru dari dashboard |
   | WiFi Save | V6 | Integer (0/1) | Trigger simpan kredensial WiFi (set ke 1 untuk save & restart) |
   | Schedule Enable | V7 | Integer (0/1) | Switch enable/disable penjadwalan relay |
   | Schedule ON Time | V8 | String (HH:MM) | Input waktu ON jadwal relay |
   | Schedule OFF Time | V9 | String (HH:MM) | Input waktu OFF jadwal relay |
   | Schedule Update | V10 | Integer (0/1) | Trigger simpan jadwal (set ke 1 untuk save) |

4. Buat **Device** baru dari template tersebut, salin **Auth Token**.
5. Di aplikasi Blynk (mobile/web dashboard), buat dashboard dengan widget:
   - **Gauge/Value Display** → bind ke V0 (Suhu)
   - **Gauge/Value Display** → bind ke V1 (Kelembaban)
   - **Switch/Button** → bind ke V2 (Relay)
   - **Value Display / Label** → bind ke V3 (Status)
   - **Text Input** → bind ke V4 (WiFi SSID)
   - **Text Input** → bind ke V5 (WiFi Password)
   - **Button (momentary)** → bind ke V6 (Save WiFi, set value ke 1)
   - **Switch** → bind ke V7 (Schedule Enable)
   - **Text Input / Time Picker** → bind ke V8 (Schedule ON Time, format HH:MM)
   - **Text Input / Time Picker** → bind ke V9 (Schedule OFF Time, format HH:MM)
   - **Button (momentary)** → bind ke V10 (Save Schedule, set value ke 1)
6. Jika ingin notifikasi otomatis, buat event bernama `high_temp` di Blynk Console karena kode memanggil `Blynk.logEvent("high_temp", ...)` saat suhu melebihi batas.

## Konfigurasi Kode

Sebelum upload, ubah bagian berikut di `smart-room-sma.ino`:

```cpp
#define BLYNK_TEMPLATE_ID   "TMPLxxxxxxx"     // dari Blynk.Console
#define BLYNK_TEMPLATE_NAME "Smart Room SMA"
#define BLYNK_AUTH_TOKEN    "ISI_AUTH_TOKEN_ANDA"

char ssid[] = "NAMA_WIFI_ANDA";
char pass[] = "PASSWORD_WIFI_ANDA";

#define DHT_PIN 3
#define RELAY_PIN 7
#define DHTTYPE DHT22
#define HIGH_TEMP 33
#define PB1_PIN 0
#define PB2_PIN 1
#define OLED_SDA 8
#define OLED_SCL 9
#define SEND_INTERVAL 5000L
#define BUZZER_PIN 6
```

Jika ingin mengubah ambang alarm, sesuaikan nilai `HIGH_TEMP` di awal kode (default: 33°C). Pastikan `BUZZER_PIN` dan pin OLED juga sesuai dengan koneksi hardware Anda.

### Fitur Penyimpanan WiFi Credentials

Kode menggunakan **ESP32 Preferences (NVS)** untuk menyimpan kredensial WiFi. Saat pertama kali dijalankan, kode akan menggunakan SSID & password hardcoded di atas. Setelah terhubung ke Blynk, Anda dapat mengganti WiFi melalui dashboard:

1. Masukkan SSID baru di widget **V4** dan password di widget **V5**.
2. Tekan tombol **Save WiFi (V6)** — kredensial akan tersimpan ke NVS dan ESP32 akan restart.
3. Setelah restart, ESP32 akan otomatis membaca kredensial dari NVS. Kredensial hardcoded akan diabaikan jika NVS sudah terisi.

### Fitur Penjadwalan Relay

Relay dapat dikontrol otomatis berdasarkan waktu tertentu:

1. Aktifkan **Schedule Enable (V7)** dari dashboard.
2. Masukkan waktu ON (format HH:MM) di **V8** dan waktu OFF di **V9**.
3. Tekan **Save Schedule (V10)** untuk menyimpan jadwal ke NVS.
4. ESP32 akan memeriksa waktu setiap menit (via NTP) dan mengaktifkan/mematikan relay sesuai jadwal.
5. Jadwal tetap tersimpan meskipun ESP32 restart atau mati listrik.

> ⚠️ Jadwal hanya berfungsi jika NTP time sudah tersinkronisasi. Pastikan koneksi internet stabil.

### Tampilan OLED

OLED menampilkan informasi secara real-time:

- **Baris 1**: Judul "== SMART ROOM =="
- **Baris 2**: Suhu (°C), Kelembaban (%), dan status Relay (ON/OFF)
- **Baris 3**: SSID WiFi yang sedang terhubung
- **Baris 4-5** (jika schedule enabled): Waktu saat ini, jadwal ON dan OFF

## Cara Upload

1. Hubungkan ESP32-C3 Mini ke komputer via USB.
2. Pilih **Board**: ESP32C3 Dev Module, **Port** sesuai COM/tty yang muncul.
3. Klik **Upload**.
4. Buka **Serial Monitor** (baud rate `115200`) untuk melihat log koneksi WiFi/Blynk, pembacaan sensor, dan sinkronisasi NTP.
5. OLED akan menampilkan status koneksi WiFi saat booting, kemudian menampilkan suhu, kelembaban, status relay, dan jadwal secara berkala (update suhu setiap 5 detik sesuai `SEND_INTERVAL`).
6. Jika tombol manual dipakai, PB1 akan mengaktifkan relay dan PB2 akan mematikannya, lalu status relay akan disinkronkan ke aplikasi Blynk secara otomatis.