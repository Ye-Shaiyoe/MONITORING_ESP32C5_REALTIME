#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>
#include "config.h"

// Pin definitions
#define SDA_PIN  8
#define SCL_PIN  9
#define RX1_PIN  4
#define TX1_PIN  5

// Sensor value range limits
#define SUHU_MIN      10.0f
#define SUHU_MAX      35.0f
#define RH_MIN        35.0f
#define RH_MAX        90.0f
#define TEKANAN_MIN  800.0f
#define TEKANAN_MAX 1100.0f

// Timing intervals (ms)
#define LCD_INTERVAL    100UL   // update LCD lebih cepat (was 500ms)
#define REINIT_INTERVAL 60000UL

#if DEBUG_MODE
  #define DBG(...)  Serial.printf(__VA_ARGS__)
  #define DBGLN(x)  Serial.println(x)
#else
  #define DBG(...)
  #define DBGLN(x)
#endif

LiquidCrystal_I2C lcd(0x27, 16, 2);
HardwareSerial    sensorSerial(1);

typedef struct {
  char satuan[5];
  char nilai[12];
  bool valid;
} PaketSensor;

PaketSensor bufRH   = {0};
PaketSensor bufTemp = {0};
PaketSensor bufBar  = {0};

typedef enum { WAIT_STX, READ_SATUAN, READ_NILAI } ParseState;
ParseState parseState = WAIT_STX;
int        parseIdx   = 0;
char       tmpSatuan[5]  = {0};
char       tmpNilai[12]  = {0};

float kelembapan = 0.0f;
float suhu       = 0.0f;
float tekanan    = 0.0f;
bool  dataReady  = false;

unsigned long lastLCD    = 0;
unsigned long lastReinit = 0;
unsigned long lastSend   = 0;  // throttle pengiriman ke server

byte wifiIcon[8]    = {0x1C, 0x0A, 0x11, 0x00, 0x04, 0x00, 0x04, 0x00};
byte barLevel[6][8] = {
  {0},
  {0, 0, 0, 0, 0, 0, 0, 0x1F},
  {0, 0, 0, 0, 0, 0, 0x1F, 0x1F},
  {0, 0, 0, 0, 0, 0x1F, 0x1F, 0x1F},
  {0, 0, 0, 0, 0x1F, 0x1F, 0x1F, 0x1F},
  {0, 0, 0, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}
};

void initLCD() {
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, wifiIcon);
  for (int j = 1; j <= 5; j++) lcd.createChar(j, barLevel[j]);
  lcd.createChar(6, barLevel[0]);
}

void connectToWiFi() {
  WiFi.disconnect(true);
  delay(100);

  DBGLN("[WiFi] Scanning jaringan 5GHz...");
  int n = WiFi.scanNetworks();

  uint8_t bssid5G[6] = {0};
  int     channel5G  = 0;
  bool    found5G    = false;

  for (int x = 0; x < n; x++) {
    if (WiFi.SSID(x) == WIFI_SSID && WiFi.channel(x) >= 36) {
      memcpy(bssid5G, WiFi.BSSID(x), 6);
      channel5G = WiFi.channel(x);
      found5G   = true;
      DBG("[WiFi] Ketemu 5GHz — CH:%d BSSID:%02X:%02X:%02X:%02X:%02X:%02X\n",
          channel5G,
          bssid5G[0], bssid5G[1], bssid5G[2],
          bssid5G[3], bssid5G[4], bssid5G[5]);
      break;
    }
  }
  WiFi.scanDelete();

  if (found5G) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, channel5G, bssid5G);
  } else {
    DBGLN("[WiFi] 5GHz tidak ditemukan, konek biasa...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) delay(500);
}

void sendToServer() {
  if (WiFi.status() != WL_CONNECTED) return;

  // Throttle: tunggu minimal SEND_INTERVAL sebelum kirim lagi
  if (millis() - lastSend < SEND_INTERVAL) {
    DBG("[HTTP] Throttle aktif, sisa %lu ms\n", SEND_INTERVAL - (millis() - lastSend));
    return;
  }
  lastSend = millis();

  char url[256];
  snprintf(url, sizeof(url),
           "https://%s%s?suhu=%.1f&kelembapan=%.1f&tekanan=%.1f",
           SERVER_HOST, SERVER_PATH, suhu, kelembapan, tekanan);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  DBG("[HTTP] Kirim ke: %s\n", url);
  http.begin(client, url);

  int httpCode = http.GET();
  if (httpCode > 0) DBGLN("[HTTP] Response: " + http.getString());
  else              DBG("[HTTP] Error: %s\n", http.errorToString(httpCode).c_str());

  http.end();
}

void updateLCD() {
  if (millis() - lastLCD < LCD_INTERVAL) return;
  lastLCD = millis();

  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    int bars = constrain(map(rssi, -90, -30, 0, 5), 0, 5);

    lcd.setCursor(0, 0);
    lcd.write((uint8_t)0);
    lcd.print(" ");
    for (int k = 1; k <= 5; k++) lcd.write((uint8_t)(k <= bars ? k : 6));
    lcd.print(rssi);
    lcd.print("dBm  ");

    lcd.setCursor(0, 1);
    lcd.print(kelembapan, 1); lcd.print("|");
    lcd.print(suhu,       1); lcd.print("|");
    lcd.print(tekanan,    1);
    lcd.print("   ");

  } else {
    lcd.setCursor(0, 0); lcd.print("WiFi: TERPUTUS  ");
    lcd.setCursor(0, 1); lcd.print("Reconnect...    ");
  }
}

float parseNilai(char* s) {
  char buf[12];
  strncpy(buf, s, 11);
  buf[11] = '\0';

  long raw = atol(buf);
  int  pol = (int)(raw / 1000000000L);
  int  dp  = (int)((raw / 100000000L) % 10);
  long num = raw % 100000000L;

  float val = (float)num / powf(10.0f, (float)dp);
  if (pol) val *= -1.0f;
  return val;
}

// Kode satuan: 41xx = Kelembapan, 42xx = Suhu, 43xx = Tekanan
void dispatchPaket(char* satuan, char* nilai) {
  int kode  = atoi(satuan);
  int jenis = kode / 100;

  PaketSensor* target = nullptr;
  if      (jenis == 41) target = &bufRH;
  else if (jenis == 42) target = &bufTemp;
  else if (jenis == 43) target = &bufBar;
  else {
    DBG("[Parser] Kode satuan tidak dikenal: %s\n", satuan);
    return;
  }

  strncpy(target->satuan, satuan, 4); target->satuan[4] = '\0';
  strncpy(target->nilai,  nilai,  11); target->nilai[11] = '\0';
  target->valid = true;

  if (bufRH.valid && bufTemp.valid && bufBar.valid) {
    dataReady = true;
  }
}

void parseSensorData(uint8_t rc) {
  switch (parseState) {

    case WAIT_STX:
      if (rc == 0x02) {
        parseIdx   = 0;
        parseState = READ_SATUAN;
        memset(tmpSatuan, 0, sizeof(tmpSatuan));
        memset(tmpNilai,  0, sizeof(tmpNilai));
      }
      break;

    case READ_SATUAN:
      tmpSatuan[parseIdx++] = (char)rc;
      if (parseIdx == 4) {
        tmpSatuan[4] = '\0';
        parseIdx     = 0;
        parseState   = READ_NILAI;
      }
      break;

    case READ_NILAI:
      tmpNilai[parseIdx++] = (char)rc;
      if (parseIdx == 11) {
        tmpNilai[11] = '\0';
        dispatchPaket(tmpSatuan, tmpNilai);
        parseIdx   = 0;
        parseState = WAIT_STX;
      }
      break;
  }
}

void processIfReady() {
  if (!dataReady) return;
  dataReady = false;

  kelembapan = parseNilai(bufRH.nilai);
  suhu       = parseNilai(bufTemp.nilai);
  tekanan    = parseNilai(bufBar.nilai);

  DBG("[Sensor] Suhu: %.1f C | Kelembapan: %.1f%% | Tekanan: %.1f hPa\n",
      suhu, kelembapan, tekanan);

  if (suhu       >= SUHU_MIN    && suhu       <= SUHU_MAX    &&
      kelembapan >= RH_MIN      && kelembapan <= RH_MAX      &&
      tekanan    >= TEKANAN_MIN && tekanan    <= TEKANAN_MAX) {
    sendToServer();
  } else {
    DBGLN("[Filter] Data di luar range, tidak dikirim.");
  }

  memset(&bufRH,   0, sizeof(bufRH));
  memset(&bufTemp, 0, sizeof(bufTemp));
  memset(&bufBar,  0, sizeof(bufBar));
}

void setup() {
  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 3000) delay(10);
  delay(200);

  Wire.begin(SDA_PIN, SCL_PIN);
  initLCD();
  sensorSerial.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WIFI_HOSTNAME);

  // Aktifkan hardware watchdog — auto-restart jika loop hang > WDT_TIMEOUT_SEC
  esp_task_wdt_config_t wdt_cfg = {
    .timeout_ms     = WDT_TIMEOUT_SEC * 1000,
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  esp_task_wdt_reconfigure(&wdt_cfg);
  esp_task_wdt_add(NULL);
  DBGLN("[WDT] Watchdog aktif, timeout " + String(WDT_TIMEOUT_SEC) + " detik");

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Scanning 5GHz...");
  lcd.setCursor(0, 1); lcd.print("Tunggu...       ");

  connectToWiFi();

  // Reinit I2C & LCD setelah WiFi scan selesai
  Wire.end();
  delay(50);
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(50);
  initLCD();
  lcd.clear();

  if (WiFi.status() == WL_CONNECTED) {
    DBG("[WiFi] Terhubung : %s\n", WiFi.localIP().toString().c_str());
    DBG("[WiFi] Channel   : %d\n", WiFi.channel());
    DBG("[WiFi] RSSI      : %d dBm\n", WiFi.RSSI());
  } else {
    DBGLN("[WiFi] Gagal konek");
  }
}

void loop() {
  esp_task_wdt_reset();  // feed watchdog — kalau baris ini tidak tercapai dalam WDT_TIMEOUT_SEC, ESP32 restart

  // Reinit LCD berkala untuk cegah glitch
  if (millis() - lastReinit >= REINIT_INTERVAL) {
    lastReinit = millis();
    initLCD();
    DBGLN("[LCD] Reinit selesai");
  }

  updateLCD();

  if (WiFi.status() != WL_CONNECTED) connectToWiFi();

  while (sensorSerial.available()) {
    parseSensorData((uint8_t)sensorSerial.read());
  }

  processIfReady();
}
