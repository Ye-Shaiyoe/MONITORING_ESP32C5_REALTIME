/*
 * ╔══════════════════════════════════════════════════════════╗
 * ║  ESP32-C5  —  LAB K46  —  Fluke 1620A DewK             ║
 * ║  Data : Suhu + Kelembapan (Fluke tidak punya barometer) ║
 * ╠══════════════════════════════════════════════════════════╣
 * ║  MODE — uncomment SATU saja:                            ║
 * ║  FLUKE_AUTO_PRINT   → Fluke streaming otomatis          ║
 * ║                       Set di menu Fluke:                ║
 * ║                       System → Comm → PRINT:ON          ║
 * ║                       SER PER: 10s (atau sesuai kebutuhan)║
 * ║  FLUKE_COMMAND_MODE → ESP32 query MEAS? 1 tiap 10 detik ║
 * ║                       Perlu kabel TX (Ring 3.5mm)       ║
 * ╠══════════════════════════════════════════════════════════╣
 * ║  Wiring 3.5mm TRS Fluke 1620A:                         ║
 * ║    Tip   → GPIO 4 (RX1 ESP32) — TX dari Fluke          ║
 * ║    Ring  → GPIO 5 (TX1 ESP32) — RX ke Fluke (cmd mode) ║
 * ║    Sleeve→ GND                                          ║
 * ║  Wajib pakai RS-232 to TTL converter (MAX3232)!        ║
 * ╚══════════════════════════════════════════════════════════╝
 */

// ─── Pilih mode komunikasi ────────────────────────────────────────────────────
#define FLUKE_AUTO_PRINT
// #define FLUKE_COMMAND_MODE

// ─── Library ──────────────────────────────────────────────────────────────────
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_mac.h>

// ─── Pin ──────────────────────────────────────────────────────────────────────
#define SDA_PIN   8
#define SCL_PIN   9
#define RX1_PIN   4   // Tip   3.5mm → TX Fluke masuk RX ESP32
#define TX1_PIN   5   // Ring  3.5mm → TX ESP32 keluar ke RX Fluke (command mode)

// ─── Jaringan ─────────────────────────────────────────────────────────────────
const char*  ssid     = "MAIN MASS";
const char*  password = "massaK46";
const char*  host     = "percobaanta1hares.my.id";
const String path     = "/F01d3rD4t4k0D3L4BBPSUML/kirimdata/kirimdata5.php";

// ─── Hardware ─────────────────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);
HardwareSerial    sensorSerial(1);

// ─── Data Sensor ──────────────────────────────────────────────────────────────
float suhu           = 0.0;
float kelembapan     = 0.0;
String  flukeLine    = "";
bool    flukeDataReady = false;

// ─── HTTP FreeRTOS ────────────────────────────────────────────────────────────
struct HttpPayload {
  float suhu;
  float kelembapan;
};

QueueHandle_t httpQueue;
unsigned long lastSend     = 0;
unsigned long sendInterval = 5000;

// ─── LCD ──────────────────────────────────────────────────────────────────────
unsigned long lastLCD     = 0;
char lcdBuf[2][17]        = {"                ", "                "};

byte wifiIcon[8] = {0x1C, 0x0A, 0x11, 0x00, 0x04, 0x00, 0x04, 0x00};
byte barLevel[6][8] = {
  {0},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F},
  {0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F},
  {0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}
};

// ─────────────────────────────────────────────────────────────────────────────
// LCD FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────────

void initLCD() {
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, wifiIcon);
  for (int j = 1; j <= 5; j++) lcd.createChar(j, barLevel[j]);
  lcd.createChar(6, barLevel[0]);   // char 6 = bar kosong
  memset(lcdBuf, ' ', sizeof(lcdBuf));
}

void lcdPrint(int row, const char* text) {
  char padded[17];
  snprintf(padded, sizeof(padded), "%-16s", text);
  if (memcmp(lcdBuf[row], padded, 16) == 0) return;
  memcpy(lcdBuf[row], padded, 16);
  lcd.setCursor(0, row);
  lcd.print(padded);
}

void updateLCD() {
  if (millis() - lastLCD < 100) return;
  lastLCD = millis();
  char row[17];

  // ── Row 0: WiFi status + RSSI ─────────────────────────────────────────────
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    int bars = constrain(map(rssi, -90, -30, 0, 5), 0, 5);

    snprintf(row, sizeof(row), "  %-5s%3ddBm", "", rssi);
    lcdPrint(0, row);
    lcd.setCursor(0, 0);
    lcd.write((uint8_t)0);   // ikon WiFi
    lcd.print(" ");
    for (int k = 1; k <= 5; k++) lcd.write((uint8_t)(k <= bars ? k : 6));
  } else {
    lcdPrint(0, "WiFi: TERPUTUS  ");
  }

  // ── Row 1: Suhu + Kelembapan (tanpa tekanan) ──────────────────────────────
  snprintf(row, sizeof(row), "T:%.1fC RH:%.1f%%", suhu, kelembapan);
  lcdPrint(1, row);
}

// ─────────────────────────────────────────────────────────────────────────────
// HTTP TASK (FreeRTOS)
// ─────────────────────────────────────────────────────────────────────────────

void httpTask(void* param) {
  HttpPayload p;
  for (;;) {
    if (xQueueReceive(httpQueue, &p, portMAX_DELAY) != pdTRUE) continue;

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[HTTP] Skip — WiFi putus");
      continue;
    }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10);

    // URL tanpa tekanan — Fluke 1620A tidak punya barometer
    String url = "https://" + String(host) + path
                 + "?suhu="       + String(p.suhu,       1)
                 + "&kelembapan=" + String(p.kelembapan, 1);

    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(10000);

    int code = http.GET();
    if (code > 0)
      Serial.println("[HTTP] OK "  + String(code) + " — " + http.getString());
    else
      Serial.println("[HTTP] ERR — " + http.errorToString(code));

    http.end();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// WIFI
// ─────────────────────────────────────────────────────────────────────────────

void connectToWiFi() {
  WiFi.disconnect(true);
  delay(100);

  Serial.println("[WiFi] Scanning...");
  int n = WiFi.scanNetworks();

  uint8_t bssid5G[6] = {0};
  int     channel5G  = 0;
  bool    found5G    = false;

  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == ssid && WiFi.channel(i) >= 36) {
      memcpy(bssid5G, WiFi.BSSID(i), 6);
      channel5G = WiFi.channel(i);
      found5G   = true;
      Serial.printf("[WiFi] 5GHz CH:%d BSSID:%02X:%02X:%02X:%02X:%02X:%02X\n",
                    channel5G,
                    bssid5G[0], bssid5G[1], bssid5G[2],
                    bssid5G[3], bssid5G[4], bssid5G[5]);
      break;
    }
  }
  WiFi.scanDelete();

  if (found5G)
    WiFi.begin(ssid, password, channel5G, bssid5G);
  else {
    Serial.println("[WiFi] 5GHz tidak ditemukan, konek biasa...");
    WiFi.begin(ssid, password);
  }

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) delay(500);
}

// ─────────────────────────────────────────────────────────────────────────────
// FLUKE 1620A — ASCII PARSER
// ─────────────────────────────────────────────────────────────────────────────

/*
 * Format response Fluke 1620A yang mungkin:
 *   "23.50,50.90"          — MEAS? 1 (comma)
 *   "23.50, 50.90"         — MEAS? 1 (comma + spasi)
 *   "23.50\t50.90"         — tab-separated
 *   "  23.50   50.90  "    — space-separated
 *   format lain tergantung firmware Fluke
 *
 * Lihat "[Fluke] Raw:" di Serial Monitor untuk tahu format persis.
 * Jika format berbeda, sesuaikan parseFlukeResponse() di bawah.
 */

void parseFlukeResponse(String& line) {
  line.trim();
  if (line.length() == 0) return;

  Serial.println("[Fluke] Raw: \"" + line + "\"");

  /*
   * Format Fluke 1620A auto-print (confirmed dari Serial Monitor):
   *   "07-23-2026 08:31:30, 23.19,C,  50.5,%,       0,C,       0,%"
   *    └── datetime ──────┘ └─T──┘└┘└──RH──┘└┘└── CH2 (0) ────────┘
   *
   * Struktur per koma:
   *   [0] datetime (sebelum koma 1)
   *   [1] suhu          ← ambil ini   (setelah koma ke-1)
   *   [2] satuan suhu C ← skip
   *   [3] kelembapan    ← ambil ini   (setelah koma ke-3)
   *   [4] satuan RH %   ← skip
   *   [5..8] channel 2  ← abaikan (0 karena tidak ada sensor)
   */

  // Koma ke-1: setelah datetime ("08:31:30,")
  int c1 = line.indexOf(',');
  if (c1 < 0) { Serial.println("[Fluke] Koma-1 tidak ditemukan, skip."); return; }

  // Suhu: substring setelah koma-1, String.toFloat() otomatis skip spasi
  float t = line.substring(c1 + 1).toFloat();

  // Koma ke-2: setelah nilai suhu ("23.19,")
  int c2 = line.indexOf(',', c1 + 1);
  if (c2 < 0) { Serial.println("[Fluke] Koma-2 tidak ditemukan, skip."); return; }

  // Koma ke-3: setelah satuan suhu ("C,")
  int c3 = line.indexOf(',', c2 + 1);
  if (c3 < 0) { Serial.println("[Fluke] Koma-3 tidak ditemukan, skip."); return; }

  // Kelembapan: substring setelah koma-3
  float rh = line.substring(c3 + 1).toFloat();

  // Sanity check range sensor (lebih lebar dari filter kirim)
  if (t < -40.0f || t > 100.0f || rh < 0.0f || rh > 100.0f) {
    Serial.printf("[Fluke] Nilai tidak wajar (T:%.3f RH:%.2f), skip.\n", t, rh);
    return;
  }

  suhu           = t;
  kelembapan     = rh;
  flukeDataReady = true;

  Serial.printf("[Fluke] Suhu: %.3f°C  RH: %.2f%%\n", suhu, kelembapan);
}

// Terima karakter dari serial, kumpulkan per baris
void readFlukeSerial() {
  while (sensorSerial.available()) {
    char c = (char)sensorSerial.read();

    if (c == '\n') {
      parseFlukeResponse(flukeLine);
      flukeLine = "";
    } else if (c != '\r') {
      if (flukeLine.length() < 64) flukeLine += c;
    }
  }
}

// Kirim ke server kalau ada data baru dan sudah waktunya
void processFluke() {
  if (!flukeDataReady) return;
  flukeDataReady = false;

  unsigned long now = millis();
  if (now - lastSend < sendInterval) return;
  lastSend     = now;
  sendInterval = random(5000, 7001);

  bool inRange = (suhu       >= 10.0f && suhu       <= 35.0f) &&
                 (kelembapan >= 35.0f && kelembapan <= 90.0f);

  if (inRange) {
    HttpPayload payload = { suhu, kelembapan };
    if (xQueueSend(httpQueue, &payload, 0) == pdTRUE)
      Serial.printf("[HTTP] Antri — next %lus\n", sendInterval / 1000);
    else
      Serial.println("[HTTP] Queue penuh, skip.");
  } else {
    Serial.printf("[Filter] Di luar range — T:%.1f°C RH:%.1f%% — skip.\n",
                  suhu, kelembapan);
  }
}

// ─── Command mode: ESP32 aktif query Fluke ────────────────────────────────────
#ifdef FLUKE_COMMAND_MODE
unsigned long lastFlukeQuery        = 0;
const unsigned long QUERY_INTERVAL  = 10000;   // ms — query tiap 10 detik

void queryFluke() {
  if (millis() - lastFlukeQuery < QUERY_INTERVAL) return;
  lastFlukeQuery = millis();
  sensorSerial.print("MEAS? 1\r\n");
  Serial.println("[Fluke] Query: MEAS? 1");
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);
  delay(200);

  Wire.begin(SDA_PIN, SCL_PIN);
  initLCD();

  // 9600 baud default Fluke 1620A — sesuaikan jika diubah di menu Fluke
  // invert=true wajib: Fluke kirim RS-232 (logic terbalik dari TTL)
  sensorSerial.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN, true);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("LAB_K46");

  httpQueue = xQueueCreate(5, sizeof(HttpPayload));
  xTaskCreate(httpTask, "httpTask", 8192, NULL, 1, NULL);

  lcdPrint(0, "Scanning 5GHz...");
  lcdPrint(1, "Tunggu...       ");
  connectToWiFi();

  // Re-init I2C + LCD setelah WiFi selesai
  Wire.end();   delay(50);
  Wire.begin(SDA_PIN, SCL_PIN); delay(50);
  initLCD();
  lcd.clear();

  // Print MAC
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf("[MAC]  STA        : %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Terhubung  : %s\n",    WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] Channel    : %d\n",    WiFi.channel());
    Serial.printf("[WiFi] RSSI       : %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("[WiFi] Gagal konek");
  }

  Serial.println();

#ifdef FLUKE_AUTO_PRINT
  Serial.println("[Mode] FLUKE_AUTO_PRINT");
  Serial.println("       Pastikan di menu Fluke:");
  Serial.println("         System → Comm Setting → Serial");
  Serial.println("         PRINT  : ON");
  Serial.println("         SER PER: 10s (atau sesuai kebutuhan)");
  Serial.println("         BAUD   : 9600");
#endif

#ifdef FLUKE_COMMAND_MODE
  Serial.println("[Mode] FLUKE_COMMAND_MODE");
  Serial.println("       ESP32 query MEAS? 1 tiap 10 detik");
  Serial.println("       Pastikan kabel Ring (3.5mm) tersambung ke GPIO 5");
#endif

  Serial.println("\n[Fluke] Menunggu data dari Fluke 1620A DewK...\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────────────────────────────────

void loop() {
  updateLCD();

  if (WiFi.status() != WL_CONNECTED) connectToWiFi();

  readFlukeSerial();
  processFluke();

#ifdef FLUKE_COMMAND_MODE
  queryFluke();
#endif

  delay(2);
}
