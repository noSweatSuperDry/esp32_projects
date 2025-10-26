#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include <WebServer.h>

// ================== USER CONFIG ==================
#define WIFI_SSID        "xxx"
#define WIFI_PASS        "xxx"

#define OPENWEATHER_API_KEY "xxx"
// Use your location:
#define LATITUDE   xxx   // Helsinki example
#define LONGITUDE  xxx
// Or you can switch

// Matrix setup
#define DATA_PIN   7
#define MATRIX_W   16
#define MATRIX_H   16
#define NUM_LEDS   (MATRIX_W * MATRIX_H)
#define BRIGHTNESS 32

// Update cadence
const unsigned long WEATHER_REFRESH_SEC = 3600;  // 1 hour

// Timezone
static const char* TZ_EUROPE_HELSINKI = "EET-2EEST,M3.5.0/03,M10.5.0/04";
// =================================================

Adafruit_NeoPixel strip(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

// ------------ Color helpers ------------
inline uint32_t C(uint8_t r, uint8_t g, uint8_t b) { return strip.Color(r, g, b); }
static void rgbFromColor(uint32_t col, uint8_t &r, uint8_t &g, uint8_t &b) {
  r = (col >> 16) & 0xFF; g = (col >> 8) & 0xFF; b = col & 0xFF;
}
static String hexColor(uint32_t col) {
  uint8_t r,g,b; rgbFromColor(col,r,g,b);
  char buf[8]; snprintf(buf, sizeof(buf), "#%02X%02X%02X", r,g,b);
  return String(buf);
}

// HSV (0..360, 0..1, 0..1) to RGB 0..255
static void hsvToRgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
  while (h < 0) h += 360.f;
  while (h >= 360) h -= 360.f;
  float c = v * s;
  float x = c * (1 - fabsf(fmodf(h / 60.f, 2.f) - 1));
  float m = v - c;
  float r1=0,g1=0,b1=0;
  if      (h < 60)  { r1=c; g1=x; b1=0; }
  else if (h < 120) { r1=x; g1=c; b1=0; }
  else if (h < 180) { r1=0; g1=c; b1=x; }
  else if (h < 240) { r1=0; g1=x; b1=c; }
  else if (h < 300) { r1=x; g1=0; b1=c; }
  else              { r1=c; g1=0; b1=x; }
  r = (uint8_t)roundf((r1 + m) * 255);
  g = (uint8_t)roundf((g1 + m) * 255);
  b = (uint8_t)roundf((b1 + m) * 255);
}

// 4 hourly-cycling colors (distinct hues)
uint32_t colHour = 0, colMin = 0, colIcon = 0, colTemp = 0;
int lastColorHour = -1;
void setHourlyColors(int hour) {
  if (hour == lastColorHour) return;
  lastColorHour = hour;
  float base = fmodf(hour * 37.0f, 360.0f);
  float hues[4] = { base, fmodf(base + 90.f, 360.f), fmodf(base + 180.f, 360.f), fmodf(base + 270.f, 360.f) };
  uint8_t r,g,b;
  hsvToRgb(hues[0], 1.0f, 1.0f, r, g, b); colHour = C(r,g,b);
  hsvToRgb(hues[1], 1.0f, 1.0f, r, g, b); colMin  = C(r,g,b);
  hsvToRgb(hues[2], 1.0f, 1.0f, r, g, b); colIcon = C(r,g,b);
  hsvToRgb(hues[3], 1.0f, 1.0f, r, g, b); colTemp = C(r,g,b);
}

// ------------ State ------------
float  g_tempC = NAN;
String g_weatherMain = "";
unsigned long g_lastWeatherFetch = 0;

// Blinking bar state (row y=9)
bool     g_barVisible = false;
uint32_t g_barColor   = 0;
int      g_lastSecond = -1;

// ------------ Serpentine mapping + 180° rotation ------------
// 180° rotation (invert x & y) + mirroring fix (odd: L->R, even: R->L).
uint16_t xy2i(uint8_t x, uint8_t y) {
  x = MATRIX_W - 1 - x;
  y = MATRIX_H - 1 - y;
  bool oddRow = y & 1;
  if (oddRow) return y * MATRIX_W + x;                       // left -> right
  else        return y * MATRIX_W + (MATRIX_W - 1 - x);      // right -> left
}

inline void setXY(uint8_t x, uint8_t y, uint32_t color) {
  if (x < MATRIX_W && y < MATRIX_H) strip.setPixelColor(xy2i(x, y), color);
}
inline void clearAll() { strip.clear(); }

// ------------ Fonts ------------
const uint8_t DIGIT_3x5[10][5] PROGMEM = {
  {0b111,0b101,0b101,0b101,0b111},
  {0b010,0b110,0b010,0b010,0b111},
  {0b111,0b001,0b111,0b100,0b111},
  {0b111,0b001,0b111,0b001,0b111},
  {0b101,0b101,0b111,0b001,0b001},
  {0b111,0b100,0b111,0b001,0b111},
  {0b111,0b100,0b111,0b101,0b111},
  {0b111,0b001,0b001,0b001,0b001},
  {0b111,0b101,0b111,0b101,0b111},
  {0b111,0b101,0b111,0b001,0b111}
};

const uint8_t ICON_CLEAR_6x6[6] PROGMEM = {
  0b001100, 0b010010, 0b100001, 0b100001, 0b010010, 0b001100
};
const uint8_t ICON_CLOUDS_6x6[6] PROGMEM = {
  0b000000, 0b011100, 0b111110, 0b111110, 0b011100, 0b000000
};
const uint8_t ICON_RAIN_6x6[6] PROGMEM = {
  0b011100, 0b111110, 0b111110, 0b011100, 0b010010, 0b001000
};
const uint8_t ICON_SNOW_6x6[6] PROGMEM = {
  0b001000, 0b101010, 0b011100, 0b011100, 0b101010, 0b001000
};

// ------------ Drawing helpers ------------
void drawDigit3x5(uint8_t x, uint8_t y, uint8_t d, uint32_t color) {
  if (d > 9) return;
  for (uint8_t r = 0; r < 5; r++) {
    uint8_t line = pgm_read_byte(&DIGIT_3x5[d][r]);
    for (uint8_t c = 0; c < 3; c++) {
      if (line & (1 << (2 - c))) setXY(x + c, y + r, color);
    }
  }
}

void drawWeatherIcon6x6(uint8_t x, uint8_t y, const uint8_t* bmp, uint32_t color) {
  for (uint8_t r = 0; r < 6; r++) {
    uint8_t line = pgm_read_byte(&bmp[r]);
    for (uint8_t c = 0; c < 6; c++) {
      if (line & (1 << (5 - c))) setXY(x + c, y + r, color);
    }
  }
}

// ------------ Draw sections ------------
void drawTimeTop(uint8_t hours, uint8_t minutes) {
  const uint8_t y0 = 2; // y=0..8 top band height
  uint8_t h1 = (hours / 10) % 10;
  uint8_t h2 = hours % 10;
  uint8_t m1 = (minutes / 10) % 10;
  uint8_t m2 = minutes % 10;

  // Layout: H1(0..2) gap(3) H2(4..6) gap(7) gap(8) M1(9..11) gap(12) M2(13..15)
  drawDigit3x5(0,  y0, h1, colHour);
  drawDigit3x5(4,  y0, h2, colHour);
  drawDigit3x5(9,  y0, m1, colMin);
  drawDigit3x5(13, y0, m2, colMin);
}

// Blinking bar in gap row y=9, full width (x=0..15)
void drawBlinkBar() {
  if (!g_barVisible) return;
  for (uint8_t x = 0; x < 16; x++) setXY(x, 9, g_barColor);
}

// Bottom 6×16: icon (6×6) left, temperature right
void drawBottomBand() {
  const uint8_t xIcon = 0, yIcon = 10;
  const uint8_t* icon = ICON_CLOUDS_6x6;
  if      (g_weatherMain == "Clear")        icon = ICON_CLEAR_6x6;
  else if (g_weatherMain == "Clouds")       icon = ICON_CLOUDS_6x6;
  else if (g_weatherMain == "Rain")         icon = ICON_RAIN_6x6;
  else if (g_weatherMain == "Drizzle")      icon = ICON_RAIN_6x6;
  else if (g_weatherMain == "Thunderstorm") icon = ICON_RAIN_6x6;
  else if (g_weatherMain == "Snow")         icon = ICON_SNOW_6x6;

  drawWeatherIcon6x6(xIcon, yIcon, icon, colIcon);

  if (!isnan(g_tempC)) {
    int t = (int)round(g_tempC);
    bool neg = t < 0;
    int a = abs(t);
    int d1 = (a >= 10) ? (a / 10) % 10 : -1;
    int d2 = a % 10;

    uint8_t baseY = 10;
    if (d1 >= 0) {
      drawDigit3x5(7,  baseY, d1, colTemp);
      drawDigit3x5(10, baseY, d2, colTemp);
      if (neg) { setXY(7, baseY - 1, colTemp); setXY(8, baseY - 1, colTemp); setXY(9, baseY - 1, colTemp); }
      setXY(14, baseY, colTemp); // ° dot
    } else {
      drawDigit3x5(9, baseY, d2, colTemp);
      if (neg) { setXY(8, baseY - 1, colTemp); setXY(9, baseY - 1, colTemp); setXY(10, baseY - 1, colTemp); }
      setXY(13, baseY, colTemp);
    }
  }
}

// ------------ Time & network helpers ------------
void initTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", TZ_EUROPE_HELSINKI, 1);
  tzset();
}

bool getLocalTimeSafe(struct tm* info) {
  time_t now = time(nullptr);
  if (now < 8 * 3600 * 2) return false;
  localtime_r(&now, info);
  return true;
}

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(200);
  return WiFi.status() == WL_CONNECTED;
}

String buildWeatherURL() {
  String url = "http://api.openweathermap.org/data/2.5/weather?lat=";
  url += String(LATITUDE, 4);
  url += "&lon=";
  url += String(LONGITUDE, 4);
  url += "&units=metric&appid=";
  url += OPENWEATHER_API_KEY;
  return url;
}

bool fetchWeather() {
  if (!ensureWiFi()) return false;
  HTTPClient http;
  String url = buildWeatherURL();
  http.begin(url);
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) return false;
  if (doc["main"]["temp"].is<float>()) g_tempC = doc["main"]["temp"].as<float>();
  if (doc["weather"][0]["main"].is<const char*>()) g_weatherMain = String(doc["weather"][0]["main"].as<const char*>());
  g_lastWeatherFetch = millis() / 1000;
  return true;
}

// ------------ HTTP /status handler ------------
void handleStatus() {
  DynamicJsonDocument doc(1024);
  struct tm tinfo;
  time_t now = time(nullptr);
  bool haveTime = getLocalTimeSafe(&tinfo);

  char iso[25] = {0};
  if (haveTime) {
    snprintf(iso, sizeof(iso), "%04d-%02d-%02d %02d:%02d:%02d",
      tinfo.tm_year + 1900, tinfo.tm_mon + 1, tinfo.tm_mday,
      tinfo.tm_hour, tinfo.tm_min, tinfo.tm_sec);
  }

  JsonObject timeObj = doc.createNestedObject("time");
  timeObj["HH"]  = haveTime ? tinfo.tm_hour : -1;
  timeObj["MM"]  = haveTime ? tinfo.tm_min  : -1;
  timeObj["SS"]  = haveTime ? tinfo.tm_sec  : -1;
  timeObj["iso"] = iso;

  // ternary nullptr/float -> use if/else to satisfy C++
  if (isnan(g_tempC)) doc["temperature_c"] = nullptr;
  else                doc["temperature_c"] = g_tempC;

  doc["weather_main"] = g_weatherMain;

  JsonObject colors = doc.createNestedObject("colors");
  colors["hour"]        = hexColor(colHour);
  colors["minute"]      = hexColor(colMin);
  colors["icon"]        = hexColor(colIcon);
  colors["temperature"] = hexColor(colTemp);
  colors["bar"]         = hexColor(g_barColor);

  doc["bar_visible"] = g_barVisible;
  doc["ip"]          = WiFi.localIP().toString();
  doc["tz"]          = TZ_EUROPE_HELSINKI;
  doc["epoch"]       = (uint32_t)now;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ------------ Setup & loop ------------
void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();

  // Seed randomness for bar colors
  randomSeed((uint32_t)(micros() ^ (uint32_t)ESP.getEfuseMac()));

  ensureWiFi();
  initTime();
  fetchWeather();

  // HTTP routes
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/", HTTP_GET, [](){ server.send(200, "text/plain", "OK. Try /status"); });
  server.begin();

  Serial.print("Ready. IP: ");
  Serial.println(WiFi.localIP());
}

void drawAll() {
  clearAll();

  struct tm tinfo;
  if (getLocalTimeSafe(&tinfo)) {
    // Hourly color cycling (distinct per item, changes at top of hour)
    setHourlyColors(tinfo.tm_hour);

    // Blinking bar logic: toggle every second; ON => new random color
    if (tinfo.tm_sec != g_lastSecond) {
      g_lastSecond = tinfo.tm_sec;
      bool nextVisible = (tinfo.tm_sec % 2 == 0);
      if (nextVisible && !g_barVisible) {
        uint8_t r = random(0, 256);
        uint8_t g = random(0, 256);
        uint8_t b = random(0, 256);
        g_barColor = C(r, g, b);
      }
      g_barVisible = nextVisible;
    }

    // Draw top time (HH mm), gap bar, and bottom band
    // Top band
    const uint8_t y0 = 2;
    uint8_t h1 = (tinfo.tm_hour / 10) % 10;
    uint8_t h2 = tinfo.tm_hour % 10;
    uint8_t m1 = (tinfo.tm_min / 10) % 10;
    uint8_t m2 = tinfo.tm_min % 10;
    drawDigit3x5(0,  y0, h1, colHour);
    drawDigit3x5(4,  y0, h2, colHour);
    drawDigit3x5(9,  y0, m1, colMin);
    drawDigit3x5(13, y0, m2, colMin);

    // Gap bar
    if (g_barVisible) {
      for (uint8_t x = 0; x < 16; x++) setXY(x, 9, g_barColor);
    }
  }

  // Bottom band (icon + temperature)
  const uint8_t xIcon = 0, yIcon = 10;
  const uint8_t* icon = ICON_CLOUDS_6x6;
  if      (g_weatherMain == "Clear")        icon = ICON_CLEAR_6x6;
  else if (g_weatherMain == "Clouds")       icon = ICON_CLOUDS_6x6;
  else if (g_weatherMain == "Rain")         icon = ICON_RAIN_6x6;
  else if (g_weatherMain == "Drizzle")      icon = ICON_RAIN_6x6;
  else if (g_weatherMain == "Thunderstorm") icon = ICON_RAIN_6x6;
  else if (g_weatherMain == "Snow")         icon = ICON_SNOW_6x6;
  drawWeatherIcon6x6(xIcon, yIcon, icon, colIcon);

  if (!isnan(g_tempC)) {
    int t = (int)round(g_tempC);
    bool neg = t < 0;
    int a = abs(t);
    int d1 = (a >= 10) ? (a / 10) % 10 : -1;
    int d2 = a % 10;
    uint8_t baseY = 10;
    if (d1 >= 0) {
      drawDigit3x5(7,  baseY, d1, colTemp);
      drawDigit3x5(10, baseY, d2, colTemp);
      if (neg) { setXY(7, baseY - 1, colTemp); setXY(8, baseY - 1, colTemp); setXY(9, baseY - 1, colTemp); }
      setXY(14, baseY, colTemp);
    } else {
      drawDigit3x5(9, baseY, d2, colTemp);
      if (neg) { setXY(8, baseY - 1, colTemp); setXY(9, baseY - 1, colTemp); setXY(10, baseY - 1, colTemp); }
      setXY(13, baseY, colTemp);
    }
  }

  strip.show();
}

void loop() {
  server.handleClient();
  unsigned long nowSec = millis() / 1000;
  if (nowSec - g_lastWeatherFetch >= WEATHER_REFRESH_SEC) fetchWeather();
  drawAll();
  delay(100);
}
