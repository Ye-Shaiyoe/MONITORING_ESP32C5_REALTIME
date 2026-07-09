// Konfigurasi jaringan & server
#pragma once

// WiFi
#define WIFI_SSID "wifiKamu"
#define WIFI_PASSWORD "Password"
#define WIFI_HOSTNAME "Host"

// Server
#define SERVER_HOST "alamatserver.web.id"
#define SERVER_PATH "/urlDariWebnya/kirimdata{id}.php"

// Interval minimum antar pengiriman data ke server (ms)
#define SEND_INTERVAL 30000UL // 30 detik

// Watchdog timeout (detik) — ESP32 auto-restart kalau loop hang
#define WDT_TIMEOUT_SEC 10

#define DEBUG_MODE 1
