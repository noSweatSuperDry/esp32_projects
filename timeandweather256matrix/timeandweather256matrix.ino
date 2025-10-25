#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>

// ================== USER CONFIG ==================
#define WIFI_SSID        "xxxxxxxxxx"
#define WIFI_PASS        "xxxxxxxxxxxxxxx"

#define OPENWEATHER_API_KEY "xxxxxxxxxxxxxxxxxxx"
// Use your location:
#define LATITUDE   xxxxxxxx   // Helsinki example
#define LONGITUDE  xxxxxxxx
// Or you can switch to city name query, see buildWeatherURL()
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

// ------------ Colors ------------
inline uint32_t C(uint8_t r, uint8_t g, uint8_t b) { return strip.Color(r, g, b); }
const uint32_t COL_HOUR = 0x0000FF00;   // green
const uint32_t COL_MIN  = 0x00A000FF;   // purple
const uint32_t COL_DOT  = 0x00FF0000;   // red
const uint32_t COL_ICON = 0x0000A0FF;   // sky blue
const uint32_t COL_TEMP = 0x00FF8000;   // orange

// ------------ State ------------
float  g_tempC = NAN;
String g_weatherMain = "";
unsigned long g_lastWeatherFetch = 0;

// ------------ Serpentine mapping ------------
// Adds 180° rotation by inverting both x and y.
uint16_t xy2i(uint8_t x, uint8_t y) {
  x = MATRIX_W - 1 - x;
  y = MATRIX_H - 1 - y;
  bool oddRow = y & 1;
  if (oddRow) return y * MATRIX_W + x;
  else return y * MATRIX_W + (MATRIX_W - 1 - x);
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
void drawTimeTop(uint8_t hours, uint8_t minutes, bool showBlinkDot) {
  const uint8_t y0 = 2;
  uint8_t h1 = (hours / 10) % 10;
  uint8_t h2 = hours % 10;
  uint8_t m1 = (minutes / 10) % 10;
  uint8_t m2 = minutes % 10;

  drawDigit3x5(0,  y0, h1, COL_HOUR);
  drawDigit3x5(4,  y0, h2, COL_HOUR);
  drawDigit3x5(9,  y0, m1, COL_MIN);
  drawDigit3x5(13, y0, m2, COL_MIN);

  if (showBlinkDot) setXY(8, y0 + 1, COL_DOT);
}

void drawBottomBand() {
  const uint8_t xIcon = 0, yIcon = 10;
  const uint8_t* icon = ICON_CLOUDS_6x6;
  if      (g_weatherMain == "Clear")        icon = ICON_CLEAR_6x6;
  else if (g_weatherMain == "Clouds")       icon = ICON_CLOUDS_6x6;
  else if (g_weatherMain == "Rain")         icon = ICON_RAIN_6x6;
  else if (g_weatherMain == "Drizzle")      icon = ICON_RAIN_6x6;
  else if (g_weatherMain == "Thunderstorm") icon = ICON_RAIN_6x6;
  else if (g_weatherMain == "Snow")         icon = ICON_SNOW_6x6;

  drawWeatherIcon6x6(xIcon, yIcon, icon, COL_ICON);

  if (!isnan(g_tempC)) {
    int t = (int)round(g_tempC);
    bool neg = t < 0;
    int a = abs(t);
    int d1 = (a >= 10) ? (a / 10) % 10 : -1;
    int d2 = a % 10;

    uint8_t baseY = 10;
    if (d1 >= 0) {
      drawDigit3x5(7,  baseY, d1, COL_TEMP);
      drawDigit3x5(10, baseY, d2, COL_TEMP);
      if (neg) { setXY(7, baseY - 1, COL_TEMP); setXY(8, baseY - 1, COL_TEMP); setXY(9, baseY - 1, COL_TEMP); }
      setXY(14, baseY, COL_TEMP);
    } else {
      drawDigit3x5(9, baseY, d2, COL_TEMP);
      if (neg) { setXY(8, baseY - 1, COL_TEMP); setXY(9, baseY - 1, COL_TEMP); setXY(10, baseY - 1, COL_TEMP); }
      setXY(13, baseY, COL_TEMP);
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

// ------------ Setup & loop ------------
void setup() {
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();
  ensureWiFi();
  initTime();
  fetchWeather();
}

void drawAll() {
  clearAll();
  struct tm tinfo;
  bool blink = false;
  if (getLocalTimeSafe(&tinfo)) {
    blink = ((tinfo.tm_sec % 2) == 0);
    drawTimeTop(tinfo.tm_hour, tinfo.tm_min, blink);
  }
  drawBottomBand();
  strip.show();
}

void loop() {
  unsigned long nowSec = millis() / 1000;
  if (nowSec - g_lastWeatherFetch >= WEATHER_REFRESH_SEC) fetchWeather();
  drawAll();
  delay(200);
}
