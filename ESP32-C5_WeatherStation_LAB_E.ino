#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// Koneksi
#define SDA_PIN   8
#define SCL_PIN   9
#define RX1_PIN   4
#define TX1_PIN   5

const char* ssid     = "WifiKamu";
const char* password = "Passordnya";
const char* host     = "url website";
const String path    = "example:/3242432424/kirimd342ata/kirimdata6.php";

// Hardware 
LiquidCrystal_I2C lcd(0x27, 16, 2);
HardwareSerial sensorSerial(1);

// Variabel Sensor 
char humid_s[5] = {0}, temp_s[5] = {0}, bar_s[5] = {0};
char humid[12]  = {0}, temp[12]   = {0}, bar[12]   = {0};
int  paket = 0, i = 0, tampil = 0;
float kelembapan = 0.0, suhu = 0.0, tekanan = 0.0;
int   satuan_rh, satuan_temp, satuan_bar;
int   pol_rh, pol_temp, pol_bar;
int   dp_rh, dp_temp, dp_bar;

// Timer 
unsigned long lastLCD    = 0;
unsigned long lastReinit = 0;
const unsigned long LCD_INTERVAL    = 500;      // update LCD tiap 500ms
const unsigned long REINIT_INTERVAL = 60000;    // reinit LCD tiap 1 menit (lebih agresif)

// Custom LCD Characters 
byte wifiIcon[8]    = {0x1C, 0x0A, 0x11, 0x00, 0x04, 0x00, 0x04, 0x00};
byte barLevel[6][8] = {
  {0},
  {0, 0, 0, 0, 0, 0, 0, 0x1F},
  {0, 0, 0, 0, 0, 0, 0x1F, 0x1F},
  {0, 0, 0, 0, 0, 0x1F, 0x1F, 0x1F},
  {0, 0, 0, 0, 0x1F, 0x1F, 0x1F, 0x1F},
  {0, 0, 0, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}
};

// Init / Reinit LCD 
void initLCD() {
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, wifiIcon);
  for (int j = 1; j <= 5; j++) lcd.createChar(j, barLevel[j]);
  lcd.createChar(6, barLevel[0]);
}

// WiFi Connect Force 5GHz via BSSID Scan 
void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("LAB_K46");
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

  if (found5G) {
    WiFi.begin(ssid, password, channel5G, bssid5G);
  } else {
    Serial.println("[WiFi] 5GHz tidak ditemukan, konek biasa...");
    WiFi.begin(ssid, password);
  }

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    delay(500);
  }
}

// Kirim Data ke Server 
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
  if (httpCode > 0) {
    Serial.println("[HTTP] Response: " + http.getString());
  } else {
    Serial.println("[HTTP] Error: " + http.errorToString(httpCode));
  }
  http.end();
}

// Update LCD 
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

// Parse Data Sensor MHB-382SD 
void parseSensorData(char rc) {
  if (rc == 0x02) { i = 0; paket++; return; }

  if      (paket == 1 && i < 4)       humid_s[i]   = rc;
  else if (paket == 1 && i - 4 < 11)  humid[i - 4] = rc;
  else if (paket == 2 && i < 4)       temp_s[i]    = rc;
  else if (paket == 2 && i - 4 < 11)  temp[i - 4]  = rc;
  else if (paket == 3 && i < 4)       bar_s[i]     = rc;
  else if (paket == 3 && i - 4 < 11) { bar[i - 4]  = rc; tampil = 1; }

  i++;

  if (i >= 14 && tampil) {
    satuan_rh   = atoi(humid_s) % 4100;
    satuan_temp = atoi(temp_s)  % 4200;
    satuan_bar  = atoi(bar_s)   % 4300;

    long h = atol(humid), t = atol(temp), b = atol(bar);

    pol_rh     = h / 1000000000;
    dp_rh      = (h / 100000000) % 10;
    kelembapan = (h % 100000000) / pow(10.0, dp_rh);

    pol_temp   = t / 1000000000;
    dp_temp    = (t / 100000000) % 10;
    suhu       = (t % 100000000) / pow(10.0, dp_temp);

    pol_bar    = b / 1000000000;
    dp_bar     = (b / 100000000) % 10;
    tekanan    = (b % 100000000) / pow(10.0, dp_bar);

    if (pol_rh)   kelembapan *= -1;
    if (pol_temp) suhu       *= -1;
    if (pol_bar)  tekanan    *= -1;

    Serial.printf("[Sensor] Suhu: %.1f C | Kelembapan: %.1f%% | Tekanan: %.1f hPa\n",
                  suhu, kelembapan, tekanan);

    if (suhu       >= 10  && suhu       <= 35  &&
        kelembapan >= 35  && kelembapan <= 90  &&
        tekanan    >= 800 && tekanan    <= 1100) {
      sendToServer();
    } else {
      Serial.println("[Filter] Data di luar range, tidak dikirim.");
    }

    tampil = paket = 0;
    while (sensorSerial.available()) sensorSerial.read();
  }
}

// Setup 
void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);
  initLCD();

  sensorSerial.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN);

  lcd.setCursor(0, 0); lcd.print("Scanning 5GHz...");
  lcd.setCursor(0, 1); lcd.print("Tunggu...       ");

  connectToWiFi();

  // I2C bus kadang macet setelah WiFi scan 5GHz — reinit paksa di sini
  Wire.end();
  delay(50);
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(50);
  initLCD();
  lcd.clear();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Terhubung: " + WiFi.localIP().toString());
    Serial.println("[WiFi] Channel  : " + String(WiFi.channel()));
  } else {
    Serial.println("[WiFi] Gagal konek");
  }
}

// Loop 
void loop() {
  // Reinit LCD tiap 1 menit untuk antisipasi I2C hang/glitch
  if (millis() - lastReinit >= REINIT_INTERVAL) {
    lastReinit = millis();
    initLCD();
    Serial.println("[LCD] Reinit selesai");
  }

  updateLCD();

  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  while (sensorSerial.available()) {
    parseSensorData((char)sensorSerial.read());
  }

  delay(2);
}
