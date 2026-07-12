Ini adalah program mengecek suhu menggunakan arduino. Untuk TA_Kode_Pemrograman_LAB_E_FIXED.ino:

Tools: 
* ESP8266 bertugas untuk:
  - Koneksi WiFi
  - Mengirim data ke server
  - Menampilkan data ke LCD
  - Menerima data sensor melalui serial
* Tampilan ESP8266: 
---
<img width="876" height="712" alt="ESP8266-2" src="https://github.com/user-attachments/assets/d470c484-f369-4269-abf4-b1edf448ac4c" />

---
* Board: 
  - NodeMCU ESP8266
  - Wemos D1 Mini
  - ESP-12E berbasis ESP8266

* LCD I2C 16x2
-F ungsinya:
  ~ Menampilkan status WiFi
  ~ Menampilkan RSSI WiFi
  ~ Menampilkan:
  ~ Kelembapan
  ~ Suhu
  ~ Tekanan

Web Server / Database:
https://percobaanta1hares.my.id
Backend:
PHP dan MySQL

Contoh tampilan:
---

<img width="8160" height="6144" alt="1000160140" src="https://github.com/user-attachments/assets/b685ebd6-9d89-4890-ae5d-6b68f2a19a2d"/>

Dan untuk yang ESP32-C5_WeatherStation_LAB_K46-FIX.ino 
* Tools:
  - ESP32-C5
  - Support 5ghz dan 2.4ghz (Dualband)
  - Lebih kenceng dan gak delay masuk ke website
  - LCD masih sama menggunakan LCD I2C 16x2.
  - Data lebih Akurat.
* Example 
---
<img width="765" height="456" alt="ESP32-C5-DevKitC-1_v1 1_callouts" src="https://github.com/user-attachments/assets/7438c2af-b896-48a2-b513-1ff92dce1e14" />

### PIN

|LCD|ESP43|
|GND|GND|
|VCC|5V|
|SDA|GPIO8|
|SCL|GPIO9|
