#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

const char* ssid = "SUML";
const char* password = "kemendag27";
const char* host = "tubpsuml.my.id";
const String path = "/21313/32131/kirimdata/kirimdata7.php";

LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial mySerial(14, 12); // RX, TX

char humid_s[5] = {0}, temp_s[5] = {0}, bar_s[5] = {0};
char humid[12] = {0}, temp[12] = {0}, bar[12] = {0};
int paket = 0, i = 0, tampil = 0;
float kelembapan, suhu, tekanan;
int satuan_rh, satuan_temp, satuan_bar, pol_rh, pol_temp, pol_bar, dp_rh, dp_temp, dp_bar;

byte wifiIcon[8] = {0x1C, 0x0A, 0x11, 0x00, 0x04, 0x00, 0x04, 0x00};
byte barLevel[6][8] = {
  {0},
  {0,0,0,0,0,0,0,0x1F},
  {0,0,0,0,0,0,0x1F,0x1F},
  {0,0,0,0,0,0x1F,0x1F,0x1F},
  {0,0,0,0,0x1F,0x1F,0x1F,0x1F},
  {0,0,0,0x1F,0x1F,0x1F,0x1F,0x1F}
};

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname("LAB_K46");
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) delay(500);
}

void sendToServer() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url = "https://" + String(host) + path +
                 "?suhu=" + suhu + "&kelembapan=" + kelembapan + "&tekanan=" + tekanan;
    http.begin(client, url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) Serial.println(http.getString());
    http.end();
  }
}

void parseSensorData(char rc) {
  if (rc == 0x02) { i = 0; paket++; return; }

  if (paket == 1 && i < 4) humid_s[i] = rc;
  else if (paket == 1 && i - 4 < 11) humid[i - 4] = rc;
  else if (paket == 2 && i < 4) temp_s[i] = rc;
  else if (paket == 2 && i - 4 < 11) temp[i - 4] = rc;
  else if (paket == 3 && i < 4) bar_s[i] = rc;
  else if (paket == 3 && i - 4 < 11) bar[i - 4] = rc, tampil = 1;

  i++;

  if (i >= 14 && tampil) {
    satuan_rh = atoi(humid_s) % 4100;
    satuan_temp = atoi(temp_s) % 4200;
    satuan_bar = atoi(bar_s) % 4300;

    long h = atol(humid), t = atol(temp), b = atol(bar);
    pol_rh = h / 1000000000;
    dp_rh = (h / 100000000) % 10;
    kelembapan = (h % 100000000) / pow(10.0, dp_rh);

    pol_temp = t / 1000000000;
    dp_temp = (t / 100000000) % 10;
    suhu = (t % 100000000) / pow(10.0, dp_temp);

    pol_bar = b / 1000000000;
    dp_bar = (b / 100000000) % 10;
    tekanan = (b % 100000000) / pow(10.0, dp_bar);

    if (pol_rh) kelembapan *= -1;
    if (pol_temp) suhu *= -1;
    if (pol_bar) tekanan *= -1;

      if (suhu >= 10 && suhu <= 35 &&
          kelembapan >= 35 && kelembapan <= 90 &&
          tekanan >= 800 && tekanan <= 1100) 
      {
        sendToServer();
      } 
      else 
      {
        Serial.println("Data tidak memenuhi syarat, tidak dikirim.");
      }


    tampil = paket = 0;
    while (mySerial.available()) mySerial.read();
  }
}

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  lcd.init(); lcd.backlight();

  lcd.createChar(0, wifiIcon);
  for (int j = 1; j <= 5; j++) lcd.createChar(j, barLevel[j]);
  lcd.createChar(6, barLevel[0]);

  lcd.setCursor(0,0); lcd.print("Menyambung WiFi");
  lcd.setCursor(0,1); lcd.print("Waiting...");
  connectToWiFi();
  lcd.clear();

   
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    int bars = constrain(map(rssi, -90, -30, 0, 5), 0, 5);

    lcd.setCursor(0,0); lcd.write(0); lcd.print(" ");
    for (int i = 1; i <= 5; i++) lcd.write(i <= bars ? i : 6);
    lcd.print(rssi); lcd.print("dBm");

    lcd.setCursor(0,1);
    lcd.print(kelembapan,1); lcd.print("|");
    lcd.print(suhu,1); lcd.print("|");
    lcd.print(tekanan,1);

  } else {
    lcd.setCursor(0,0); lcd.print("WiFi: TERPUTUS! ");
    lcd.setCursor(0,1); lcd.print("Reconnect...    ");
    connectToWiFi();
  }

  while (mySerial.available()) parseSensorData(mySerial.read());
  delay(2);
}
