#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_mac.h>

// Pin 
#define SDA_PIN  8
#define SCL_PIN  9
#define RX1_PIN  4
#define TX1_PIN  5

// Jaringan 
const char* ssid     = "MAIN MASS";
const char* password = "massaK46";
const char* host     = "tubpsuml.my.id";
const String path    = "/Lpath/path/kirimdata2.php";

// Hardware 
LiquidCrystal_I2C lcd(0x27, 16, 2);
HardwareSerial sensorSerial(1);

//  Variabel Sensor 
typedef struct {
  char satuan[5];   // 4 char + null
  char nilai[12];   // 11 char + null
  bool valid;
} PaketSensor;

// Buffer umum — 1 paket aktif saat ini
PaketSensor bufRH   = {0};
PaketSensor bufTemp = {0};
PaketSensor bufBar  = {0};

// State parser
typedef enum { WAIT_STX, READ_SATUAN, READ_NILAI } ParseState;
ParseState parseState = WAIT_STX;
int        parseIdx   = 0;
char       tmpSatuan[5] = {0};
char       tmpNilai[12] = {0};

float kelembapan = 0.0, suhu = 0.0, tekanan = 0.0;
bool  dataReady  = false;

// ─── MAC Address 
String macSTA = "";
String macAP  = "";
String macBT  = "";

unsigned long lastLCD         = 0;
unsigned long lastReinit      = 0;
unsigned long lastMACShow     = 0;
bool          showMAC         = true;

const unsigned long LCD_INTERVAL      = 500;
const unsigned long REINIT_INTERVAL   = 60000;
const unsigned long MAC_SHOW_DURATION = 6000;

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

void readMACAddress() {
  uint8_t mac[6];
  char    buf[18];

  esp_efuse_mac_get_default(mac);
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  macSTA = String(buf);

  uint8_t macAP_raw[6]; memcpy(macAP_raw, mac, 6); macAP_raw[5] += 1;
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           macAP_raw[0], macAP_raw[1], macAP_raw[2],
           macAP_raw[3], macAP_raw[4], macAP_raw[5]);
  macAP = String(buf);

  uint8_t macBT_raw[6]; memcpy(macBT_raw, mac, 6); macBT_raw[5] += 2;
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           macBT_raw[0], macBT_raw[1], macBT_raw[2],
           macBT_raw[3], macBT_raw[4], macBT_raw[5]);
  macBT = String(buf);

  Serial.println();
  Serial.println("========================================");
  Serial.println("[MAC] WiFi STA  : " + macSTA);
  Serial.println("[MAC] WiFi AP   : " + macAP);
  Serial.println("[MAC] Bluetooth : " + macBT);
  Serial.println("========================================");
  Serial.println();
}

void showMACOnLCD() {
  lcd.setCursor(0, 0);
  lcd.print("STA:");
  lcd.print(macSTA.substring(0, 12));  // "AA:BB:CC:DD:"

  lcd.setCursor(0, 1);
  lcd.print("    ");
  lcd.print(macSTA.substring(12));     // "EE:FF"
  lcd.print("       ");
}

void connectToWiFi() {
  WiFi.disconnect(true);
  delay(100);

  Serial.println("[WiFi] Scanning jaringan 5GHz...");
  int n = WiFi.scanNetworks();

  uint8_t bssid5G[6] = {0};
  int     channel5G  = 0;
  bool    found5G    = false;

  for (int x = 0; x < n; x++) {
    if (WiFi.SSID(x) == ssid && WiFi.channel(x) >= 36) {
      memcpy(bssid5G, WiFi.BSSID(x), 6);
      channel5G = WiFi.channel(x);
      found5G   = true;
      Serial.printf("[WiFi] Ketemu 5GHz — CH:%d BSSID:%02X:%02X:%02X:%02X:%02X:%02X\n",
                    channel5G,
                    bssid5G[0], bssid5G[1], bssid5G[2],
                    bssid5G[3], bssid5G[4], bssid5G[5]);
      break;
    }
  }
  WiFi.scanDelete();

  if (found5G) WiFi.begin(ssid, password, channel5G, bssid5G);
  else {
    Serial.println("[WiFi] 5GHz tidak ditemukan, konek biasa...");
    WiFi.begin(ssid, password);
  }

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) delay(500);
}

// ───────────────
void sendToServer() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://" + String(host) + path +
               "?suhu="       + String(suhu, 1) +
               "&kelembapan=" + String(kelembapan, 1) +
               "&tekanan="    + String(tekanan, 1);

  Serial.println("[HTTP] Kirim ke: " + url);
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode > 0) Serial.println("[HTTP] Response: " + http.getString());
  else              Serial.println("[HTTP] Error: " + http.errorToString(httpCode));
  http.end();
}

void updateLCD() {
  if (millis() - lastLCD < LCD_INTERVAL) return;
  lastLCD = millis();

  if (showMAC) {
    if (millis() - lastMACShow < MAC_SHOW_DURATION) {
      showMACOnLCD();
      return;
    } else {
      showMAC = false;
      lcd.clear();
    }
  }

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

// 41xx = Kelembapan, 42xx = Suhu, 43xx = Tekanan
void dispatchPaket(char* satuan, char* nilai) {
  int kode = atoi(satuan);

  // Dua digit pertama
  int jenis = kode / 100;  // 41 = RH, 42 = Temp, 43 = Bar

  PaketSensor* target = nullptr;
  if      (jenis == 41) target = &bufRH;
  else if (jenis == 42) target = &bufTemp;
  else if (jenis == 43) target = &bufBar;
  else {
    Serial.printf("[Parser] Kode satuan tidak dikenal: %s\n", satuan);
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

  Serial.printf("[Sensor] Suhu: %.1f C | Kelembapan: %.1f%% | Tekanan: %.1f hPa\n",
                suhu, kelembapan, tekanan);
  Serial.printf("[Debug]  RH  sat:%s val:%s → %.1f\n", bufRH.satuan,   bufRH.nilai,   kelembapan);
  Serial.printf("[Debug]  Tmp sat:%s val:%s → %.1f\n", bufTemp.satuan, bufTemp.nilai,  suhu);
  Serial.printf("[Debug]  Bar sat:%s val:%s → %.1f\n", bufBar.satuan,  bufBar.nilai,   tekanan);

  if (suhu       >= 10  && suhu       <= 35  &&
      kelembapan >= 35  && kelembapan <= 90  &&
      tekanan    >= 800 && tekanan    <= 1100) {
    sendToServer();
  } else {
    Serial.println("[Filter] Data di luar range, tidak dikirim.");
  }

  // Reset buffer untuk siklus berikutnya
  memset(&bufRH,   0, sizeof(bufRH));
  memset(&bufTemp, 0, sizeof(bufTemp));
  memset(&bufBar,  0, sizeof(bufBar));
}

// ───────────────
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

  readMACAddress();

  lastMACShow = millis();
  showMAC     = true;
  showMACOnLCD();
  delay(1000);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Scanning 5GHz...");
  lcd.setCursor(0, 1); lcd.print("Tunggu...       ");

  connectToWiFi();

  Wire.end(); delay(50);
  Wire.begin(SDA_PIN, SCL_PIN); delay(50);
  initLCD();
  lcd.clear();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Terhubung : " + WiFi.localIP().toString());
    Serial.println("[WiFi] Channel   : " + String(WiFi.channel()));
    Serial.println("[WiFi] RSSI      : " + String(WiFi.RSSI()) + " dBm");
    Serial.println("[MAC]  STA       : " + macSTA);
  } else {
    Serial.println("[WiFi] Gagal konek");
  }

  lastMACShow = millis();
  showMAC     = true;
  showMACOnLCD();
}

// ───────────────
void loop() {
  if (millis() - lastReinit >= REINIT_INTERVAL) {
    lastReinit = millis();
    initLCD();
    Serial.println("[LCD] Reinit selesai");
  }

  updateLCD();

  if (WiFi.status() != WL_CONNECTED) connectToWiFi();

  while (sensorSerial.available()) {
    parseSensorData((uint8_t)sensorSerial.read());
  }

  processIfReady();

  delay(2);
}
