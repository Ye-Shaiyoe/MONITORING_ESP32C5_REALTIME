#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_mac.h>

#define SDA_PIN 8
#define SCL_PIN 9
#define RX1_PIN 4
#define TX1_PIN 5

const char*  ssid     = "MAIN MASS";
const char*  password = "massaK46";
const char*  host     = "id.rumah.web";
const String path     = "/adadeh/lala.php";

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
ParseState    parseState   = WAIT_STX;
int           parseIdx     = 0;
char          tmpSatuan[5] = {0};
char          tmpNilai[12] = {0};
unsigned long lastByteTime = 0;

float kelembapan = 0.0;
float suhu       = 0.0;
float tekanan    = 0.0;
bool  dataReady  = false;

struct HttpPayload { float suhu, kelembapan, tekanan; };

QueueHandle_t httpQueue;

unsigned long lastSend     = 0;
unsigned long sendInterval = 5000;

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

    String url = "https://" + String(host) + path
                 + "?suhu="       + String(p.suhu, 1)
                 + "&kelembapan=" + String(p.kelembapan, 1)
                 + "&tekanan="    + String(p.tekanan, 1);

    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(10000);

    int code = http.GET();
    if (code > 0) Serial.println("[HTTP] OK " + String(code) + " — " + http.getString());
    else          Serial.println("[HTTP] ERR — " + http.errorToString(code));
    http.end();
  }
}

unsigned long lastLCD = 0;

char lcdBuf[2][17] = {"                ", "                "};

byte wifiIcon[8]    = {0x1C, 0x0A, 0x11, 0x00, 0x04, 0x00, 0x04, 0x00};
byte barLevel[6][8] = {
  {0},
  {0, 0, 0, 0, 0, 0, 0,    0x1F},
  {0, 0, 0, 0, 0, 0, 0x1F, 0x1F},
  {0, 0, 0, 0, 0, 0x1F, 0x1F, 0x1F},
  {0, 0, 0, 0, 0x1F, 0x1F, 0x1F, 0x1F},
  {0, 0, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}
};

void initLCD() {
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, wifiIcon);
  for (int j = 1; j <= 5; j++) lcd.createChar(j, barLevel[j]);
  lcd.createChar(6, barLevel[0]);
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

  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    int bars = constrain(map(rssi, -90, -30, 0, 5), 0, 5);

    snprintf(row, sizeof(row), "  %-5s%3ddBm", "", rssi);
    lcdPrint(0, row);
    lcd.setCursor(0, 0); lcd.write((uint8_t)0); lcd.print(" ");
    for (int k = 1; k <= 5; k++) lcd.write((uint8_t)(k <= bars ? k : 6));
  } else {
    lcdPrint(0, "WiFi: TERPUTUS  ");
  }

  snprintf(row, sizeof(row), "%.1f|%.1f|%.1f", kelembapan, suhu, tekanan);
  lcdPrint(1, row);
}

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

  if (found5G) WiFi.begin(ssid, password, channel5G, bssid5G);
  else {
    Serial.println("[WiFi] 5GHz tidak ada, konek biasa...");
    WiFi.begin(ssid, password);
  }

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) delay(500);
}

float parseNilai(char* s) {
  char buf[12];
  strncpy(buf, s, 11); buf[11] = '\0';
  long raw = atol(buf);
  bool neg = (raw / 1000000000L) != 0;
  int  dp  = (int)((raw / 100000000L) % 10);
  long num = raw % 100000000L;
  float val = (float)num / powf(10.0f, (float)dp);
  return neg ? -val : val;
}

void dispatchPaket(char* satuan, char* nilai) {
  int jenis = atoi(satuan) / 100;

  PaketSensor* target = nullptr;
  if      (jenis == 41) target = &bufRH;
  else if (jenis == 42) target = &bufTemp;
  else if (jenis == 43) target = &bufBar;
  else { Serial.printf("[Parser] Kode tidak dikenal: %s\n", satuan); return; }

  strncpy(target->satuan, satuan, 4); target->satuan[4] = '\0';
  strncpy(target->nilai,  nilai,  11); target->nilai[11] = '\0';
  target->valid = true;

  if (bufRH.valid && bufTemp.valid && bufBar.valid) dataReady = true;
}

void parseSensorData(uint8_t rc) {
  if (parseState != WAIT_STX && millis() - lastByteTime > 200)
    parseState = WAIT_STX;
  lastByteTime = millis();

  switch (parseState) {
    case WAIT_STX:
      if (rc == 0x02) {
        parseIdx = 0; parseState = READ_SATUAN;
        memset(tmpSatuan, 0, sizeof(tmpSatuan));
        memset(tmpNilai,  0, sizeof(tmpNilai));
      }
      break;
    case READ_SATUAN:
      tmpSatuan[parseIdx++] = (char)rc;
      if (parseIdx == 4) { tmpSatuan[4] = '\0'; parseIdx = 0; parseState = READ_NILAI; }
      break;
    case READ_NILAI:
      tmpNilai[parseIdx++] = (char)rc;
      if (parseIdx == 11) {
        tmpNilai[11] = '\0';
        dispatchPaket(tmpSatuan, tmpNilai);
        parseIdx = 0; parseState = WAIT_STX;
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

  Serial.printf("[Sensor] %.1f°C  %.1f%%  %.1f hPa\n", suhu, kelembapan, tekanan);

  unsigned long now = millis();
  if (now - lastSend >= sendInterval) {
    lastSend     = now;
    sendInterval = random(5000, 7001);

    bool inRange = (suhu >= 10 && suhu <= 35) &&
                   (kelembapan >= 35 && kelembapan <= 90) &&
                   (tekanan >= 800 && tekanan <= 1100);

    if (inRange) {
      HttpPayload payload = { suhu, kelembapan, tekanan };
      if (xQueueSend(httpQueue, &payload, 0) == pdTRUE)
        Serial.printf("[HTTP]  Antri — next %lus\n", sendInterval / 1000);
    } else {
      Serial.println("[Filter] Di luar range, skip.");
    }
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
  WiFi.setHostname("LAB_K46");

  httpQueue = xQueueCreate(5, sizeof(HttpPayload));
  xTaskCreate(httpTask, "httpTask", 8192, NULL, 1, NULL);

  lcdPrint(0, "Scanning 5GHz...");
  lcdPrint(1, "Tunggu...       ");
  connectToWiFi();

  Wire.end(); delay(50);
  Wire.begin(SDA_PIN, SCL_PIN); delay(50);
  initLCD(); lcd.clear();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Terhubung  : %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] Channel    : %d\n",   WiFi.channel());
    Serial.printf("[WiFi] RSSI       : %d dBm\n\n", WiFi.RSSI());
  } else {
    Serial.println("[WiFi] Gagal konek\n");
  }
}

void loop() {
  updateLCD();

  if (WiFi.status() != WL_CONNECTED) connectToWiFi();

  while (sensorSerial.available()) parseSensorData((uint8_t)sensorSerial.read());

  processIfReady();

  delay(2);
}
