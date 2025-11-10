// =========================
// Konfigurasi Blynk (ISI SENDIRI)
// =========================
#define BLYNK_TEMPLATE_ID   "TMPLxxxxx"        // ganti dengan template ID kamu
#define BLYNK_TEMPLATE_NAME "kualitas udara"   // boleh sesuai punyamu
#define BLYNK_AUTH_TOKEN    "YOUR_BLYNK_TOKEN" // <-- GANTI pakai token Blynk kamu

// =========================
// Library
// =========================
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_SGP30.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <time.h>

// =========================
// Wi-Fi (ISI SENDIRI)
// =========================
char ssid[] = "YOUR_WIFI_NAME";     // ganti
char pass[] = "YOUR_WIFI_PASSWORD"; // ganti

// =========================
// DHT22
// =========================
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// =========================
// SGP30 (gas sensor)
// =========================
Adafruit_SGP30 sgp;

// =========================
// LCD I2C 16x2
// =========================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =========================
// Sensor Debu GP2Y1010
// =========================
#define DUST_LED_PIN 16   // LED IR sensor debu
#define DUST_VO_PIN  34   // output analog sensor debu

// =========================
// Virtual Pin Blynk
// =========================
#define VPIN_SUHU        V0
#define VPIN_KELEMBAPAN  V1
#define VPIN_ECO2        V2
#define VPIN_TVOC        V3
#define VPIN_DEBU        V4
#define VPIN_STATUS      V5

// =========================
// NTP Time
// =========================
const char* ntpServer        = "pool.ntp.org";
const long  gmtOffset_sec    = 7 * 3600;  // WIB
const int   daylightOffset_sec = 0;

unsigned long lastReadTime = 0;  // buat interval 5 detik

// ----------------------------------------------------
// Fungsi bantu: hitung absolute humidity untuk SGP30
// ----------------------------------------------------
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // rumus dari datasheet SGP30
  const float mw = 18.01534;     // molar mass of water g/mol
  const float r = 8.31447215;    // universal gas constant J/mol/K
  float tempK = temperature + 273.15;
  float ah = (6.112 * exp((17.67 * temperature) / (temperature + 243.5)) * humidity * 2.1674) / tempK;
  return (uint32_t)(ah * 1000.0);  // return dalam mg/m^3
}

// ----------------------------------------------------
// Fungsi bantu: ambil waktu dalam format string
// ----------------------------------------------------
String getFormattedDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01 00:00:00";
  }
  char buffer[25];
  strftime(buffer, 25, "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// ----------------------------------------------------
// Fungsi bantu: ambil nama hari
// ----------------------------------------------------
String getHari() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Unknown";
  }
  int wday = timeinfo.tm_wday;
  switch (wday) {
    case 0: return "Minggu";
    case 1: return "Senin";
    case 2: return "Selasa";
    case 3: return "Rabu";
    case 4: return "Kamis";
    case 5: return "Jumat";
    case 6: return "Sabtu";
  }
  return "Unknown";
}

// ----------------------------------------------------
// Tampilkan ke LCD (berganti-ganti)
// ----------------------------------------------------
void tampilkanLCD(float suhu, float lembap, int tvoc, int eco2, float debu, String status) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Suhu:");
  lcd.setCursor(0, 1);
  lcd.print(suhu, 1);
  lcd.print(" C");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Lembap:");
  lcd.setCursor(0, 1);
  lcd.print(lembap, 1);
  lcd.print(" %");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TVOC:");
  lcd.setCursor(0, 1);
  lcd.print(tvoc);
  lcd.print(" ppb");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("eCO2:");
  lcd.setCursor(0, 1);
  lcd.print(eco2);
  lcd.print(" ppm");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Debu:");
  lcd.setCursor(0, 1);
  lcd.print(debu, 2);
  lcd.print(" mg/m3");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Kualitas:");
  lcd.setCursor(0, 1);
  lcd.print(status);
  delay(2000);
}

// ----------------------------------------------------
// Kirim ke Google Sheets (web app)
// ----------------------------------------------------
void sendToGoogleSheet(float suhu, float lembap, int eco2, int tvoc, float debu, String status, String waktu, String hari) {
  HTTPClient http;

  // GANTI URL INI pakai URL Google Apps Script kamu
  String url = "https://script.google.com/macros/s/YOUR_SCRIPT_ID/exec";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String data = "{";
  data += "\"tanggaljam\":\"" + waktu + "\",";
  data += "\"hari\":\"" + hari + "\",";
  data += "\"suhu\":" + String(suhu, 1) + ",";
  data += "\"kelembapan\":" + String(lembap, 1) + ",";
  data += "\"eco2\":" + String(eco2) + ",";
  data += "\"tvoc\":" + String(tvoc) + ",";
  data += "\"debu\":" + String(debu, 2) + ",";
  data += "\"status\":\"" + status + "\"";
  data += "}";

  Serial.println("Mengirim ke Google Sheets...");
  Serial.println(data);

  int httpResponseCode = http.POST(data);
  Serial.println("Respon Google Sheet: " + String(httpResponseCode));

  if (httpResponseCode != 200) {
    String payload = http.getString();
    Serial.println("Isi error: " + payload);
  }

  http.end();
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(DUST_LED_PIN, OUTPUT);
  digitalWrite(DUST_LED_PIN, HIGH);   // LED off default

  dht.begin();

  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Menghubung WiFi");

  // Koneksi Blynk + WiFi
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // SGP30
  if (!sgp.begin()) {
    Serial.println("SGP30 tidak terdeteksi!");
    lcd.clear();
    lcd.print("SGP30 ERROR");
    while (1);   // stop di sini kalau sensor gagal
  }
  sgp.IAQinit();

  lcd.clear();
  lcd.print("Siap...");
}

// =========================
// LOOP
// =========================
void loop() {
  Blynk.run();

  // baca setiap 5 detik
  if (millis() - lastReadTime >= 5000) {
    lastReadTime = millis();

    // ---- DHT22 ----
    float suhu  = dht.readTemperature();
    float lembap = dht.readHumidity();
    if (isnan(suhu) || isnan(lembap)) {
      Serial.println("Gagal baca DHT22");
      return;
    }

    // ---- SGP30 ----
    if (!sgp.IAQmeasure()) {
      Serial.println("Gagal baca SGP30");
      return;
    }
    sgp.setHumidity(getAbsoluteHumidity(suhu, lembap));

    // ---- Sensor Debu ----
    digitalWrite(DUST_LED_PIN, LOW);
    delayMicroseconds(280);
    int adcDust = analogRead(DUST_VO_PIN);
    delayMicroseconds(40);
    digitalWrite(DUST_LED_PIN, HIGH);

    float voltage   = adcDust * (3.3 / 4095.0);  // ESP32 ADC 12-bit
    float debuMgM3  = (voltage - 0.9) / 0.5;     // rumus sederhana (sesuaikan kalibrasi)
    if (debuMgM3 < 0) debuMgM3 = 0;

    // ---- Tentukan status ----
    bool kualitasBuruk = (sgp.eCO2 > 1000 || sgp.TVOC > 500 || debuMgM3 > 2.5);
    String status = kualitasBuruk ? "Buruk" : "Baik";

    String waktu = getFormattedDateTime();
    String hari  = getHari();

    // ---- Serial Monitor ----
    Serial.println("===============");
    Serial.println("Waktu: " + waktu);
    Serial.println("Hari: " + hari);
    Serial.println("Suhu: " + String(suhu));
    Serial.println("Kelembapan: " + String(lembap));
    Serial.println("eCO2: " + String(sgp.eCO2));
    Serial.println("TVOC: " + String(sgp.TVOC));
    Serial.println("Debu: " + String(debuMgM3));
    Serial.println("Status: " + status);
    Serial.println("===============");

    // ---- LCD ----
    tampilkanLCD(suhu, lembap, sgp.TVOC, sgp.eCO2, debuMgM3, status);

    // ---- Kirim ke Blynk ----
    Blynk.virtualWrite(VPIN_SUHU, suhu);
    Blynk.virtualWrite(VPIN_KELEMBAPAN, lembap);
    Blynk.virtualWrite(VPIN_ECO2, sgp.eCO2);
    Blynk.virtualWrite(VPIN_TVOC, sgp.TVOC);
    Blynk.virtualWrite(VPIN_DEBU, debuMgM3);
    Blynk.virtualWrite(VPIN_STATUS, status);

    // ---- Kirim ke Google Sheets ----
    sendToGoogleSheet(suhu, lembap, sgp.eCO2, sgp.TVOC, debuMgM3, status, waktu, hari);
  }
}
