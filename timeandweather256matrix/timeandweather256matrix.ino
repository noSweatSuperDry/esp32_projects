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

#define OPENWEATHER_API_KEY "xxxx"
// Use your location:
#define LATITUDE   xxxx   // Helsinki example
#define LONGITUDE  xxxx
// Or you can switch

// Matrix
#define DATA_PIN   7
#define MATRIX_W   16
#define MATRIX_H   16
#define NUM_LEDS   (MATRIX_W * MATRIX_H)
#define BRIGHTNESS 32

// Cadence
const unsigned long WEATHER_REFRESH_SEC = 3600; // 1 hr

// Timezone
static const char* TZ_EUROPE_HELSINKI = "EET-2EEST,M3.5.0/03,M10.5.0/04";
// ==============================

Adafruit_NeoPixel strip(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

// --------- Helpers / Colors ----------
inline uint32_t C(uint8_t r, uint8_t g, uint8_t b){ return strip.Color(r,g,b); }
static void rgbFromColor(uint32_t col, uint8_t &r, uint8_t &g, uint8_t &b){
  r=(col>>16)&0xFF; g=(col>>8)&0xFF; b=col&0xFF;
}
static String hexColor(uint32_t col){
  uint8_t r,g,b; rgbFromColor(col,r,g,b);
  char buf[8]; snprintf(buf,sizeof(buf),"#%02X%02X%02X",r,g,b);
  return String(buf);
}
inline void clearAll(){ strip.clear(); }

// --------- 180° rotation + serpentine (odd rows L→R, even R→L) ----------
uint16_t xy2i(uint8_t x, uint8_t y){
  x = MATRIX_W - 1 - x;
  y = MATRIX_H - 1 - y;
  bool odd = y & 1;
  return odd ? (y*MATRIX_W + x) : (y*MATRIX_W + (MATRIX_W-1-x));
}
inline void setXY(uint8_t x, uint8_t y, uint32_t c){
  if(x<MATRIX_W && y<MATRIX_H) strip.setPixelColor(xy2i(x,y), c);
}

// --------- Fonts ----------
const uint8_t DIGIT_3x5[10][5] PROGMEM = {
  {0b111,0b101,0b101,0b101,0b111}, {0b010,0b110,0b010,0b010,0b111},
  {0b111,0b001,0b111,0b100,0b111}, {0b111,0b001,0b111,0b001,0b111},
  {0b101,0b101,0b111,0b001,0b001}, {0b111,0b100,0b111,0b001,0b111},
  {0b111,0b100,0b111,0b101,0b111}, {0b111,0b001,0b001,0b001,0b001},
  {0b111,0b101,0b111,0b101,0b111}, {0b111,0b101,0b111,0b001,0b111}
};

// 5×5 weather icons (tiny)
const uint8_t ICON_CLEAR_5x5[5]  PROGMEM = { 0b00100,0b01010,0b10001,0b01010,0b00100 }; // sun
const uint8_t ICON_CLOUDS_5x5[5] PROGMEM = { 0b00000,0b01110,0b11111,0b11111,0b01110 };
const uint8_t ICON_RAIN_5x5[5]   PROGMEM = { 0b00100,0b01110,0b11111,0b01010,0b00100 }; // cloud + drops
const uint8_t ICON_SNOW_5x5[5]   PROGMEM = { 0b00100,0b10101,0b01110,0b10101,0b00100 };

// --------- States ----------
float  g_tempC = NAN;
String g_weatherMain = "";
unsigned long g_lastWeatherFetch = 0;
int g_lastSecond = -1;

// --------- Network & time ----------
void initTime(){
  configTime(0,0,"pool.ntp.org","time.nist.gov");
  setenv("TZ", TZ_EUROPE_HELSINKI, 1);
  tzset();
}
bool getLocalTimeSafe(struct tm* info){
  time_t now=time(nullptr);
  if(now < 8*3600*2) return false;
  localtime_r(&now, info);
  return true;
}
bool ensureWiFi(){
  if(WiFi.status()==WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-t<20000) delay(200);
  return WiFi.status()==WL_CONNECTED;
}
String buildWeatherURL(){
  String url = "http://api.openweathermap.org/data/2.5/weather?lat=";
  url += String(LATITUDE,4); url += "&lon="; url += String(LONGITUDE,4);
  url += "&units=metric&appid="; url += OPENWEATHER_API_KEY;
  return url;
}
bool fetchWeather(){
  if(!ensureWiFi()) return false;
  HTTPClient http; http.begin(buildWeatherURL());
  int code=http.GET(); if(code!=200){ http.end(); return false; }
  DynamicJsonDocument doc(2048);
  if(deserializeJson(doc,http.getStream())){ http.end(); return false; }
  http.end();
  if(doc["main"]["temp"].is<float>()) g_tempC = doc["main"]["temp"].as<float>();
  if(doc["weather"][0]["main"].is<const char*>()) g_weatherMain = String(doc["weather"][0]["main"].as<const char*>());
  g_lastWeatherFetch = millis()/1000;
  return true;
}

// --------- Drawing primitives ----------
void drawDigit3x5(uint8_t x,uint8_t y,uint8_t d,uint32_t col){
  if(d>9) return;
  for(uint8_t r=0;r<5;r++){
    uint8_t line=pgm_read_byte(&DIGIT_3x5[d][r]);
    for(uint8_t c=0;c<3;c++) if(line & (1<<(2-c))) setXY(x+c,y+r,col);
  }
}
void drawIcon5x5(uint8_t x,uint8_t y,const uint8_t* bmp,uint32_t col){
  for(uint8_t r=0;r<5;r++){
    uint8_t line=pgm_read_byte(&bmp[r]);
    for(uint8_t c=0;c<5;c++) if(line & (1<<(4-c))) setXY(x+c,y+r,col);
  }
}

// --------- Sections (per your spec) ----------
// 1) TOP 5×16 time: HH MM (no colon), hours green, minutes purple
const uint32_t COL_HOUR = 0x0000FF00;
const uint32_t COL_MIN  = 0x009000FF;
void drawTopTime(uint8_t hh,uint8_t mm){
  // y=0..4; x layout: H1(0..2) gap3 H2(4..6) gap7 M1(8..10) gap11 M2(12..14) x15 blank
  drawDigit3x5(0, 0, (hh/10)%10, COL_HOUR);
  drawDigit3x5(4, 0, hh%10,      COL_HOUR);
  drawDigit3x5(8+1, 0, (mm/10)%10, COL_MIN);   // shift one to leave small gap column 8
  drawDigit3x5(12+1,0, mm%10,      COL_MIN);   // leave x=15 blank
}

// 2) y=5 blank (handled in clear)

// 3) MIDDLE 4×16 seconds field: fill 1 pixel each second with a new random color; reset at rollover
void clearSecondsField(){
  for(uint8_t y=6;y<=9;y++) for(uint8_t x=0;x<16;x++) setXY(x,y,0);
}
void drawSecondsField(uint8_t sec){
  // sec is 0..59. Map to linear i=sec, row=i/16 (0..3), col=i%16.
  uint8_t i=sec;
  uint8_t row=i/16, col=i%16;
  uint8_t y=6+row, x=col;
  // new random color each second
  uint32_t colr = C(random(0,256), random(0,256), random(0,256));
  setXY(x,y,colr);
}

// 4) y=10 blank

// 5) BOTTOM 5×16 weather: 5×5 icon at left, temp (3×5) at right
// ------------------ Weather display (bottom) ------------------
const uint32_t COL_ICON = 0x0000A0FF;
const uint32_t COL_TEMP = 0x00FF8000;

void drawBottomWeather() {
  // Choose icon
  const uint8_t* icon = ICON_CLOUDS_5x5;
  if      (g_weatherMain == "Clear")        icon = ICON_CLEAR_5x5;
  else if (g_weatherMain == "Clouds")       icon = ICON_CLOUDS_5x5;
  else if (g_weatherMain == "Rain")         icon = ICON_RAIN_5x5;
  else if (g_weatherMain == "Drizzle")      icon = ICON_RAIN_5x5;
  else if (g_weatherMain == "Thunderstorm") icon = ICON_RAIN_5x5;
  else if (g_weatherMain == "Snow")         icon = ICON_SNOW_5x5;

  // Icon at x=0..4, y=11..15
  drawIcon5x5(0, 11, icon, COL_ICON);

  // Temperature on right (integer °C)
  if (!isnan(g_tempC)) {
    int t = (int)round(g_tempC);
    bool neg = t < 0;
    int a = abs(t);
    int d1 = (a >= 10) ? (a / 10) % 10 : -1;
    int d2 = a % 10;
    uint8_t baseY = 11;

    if (d1 >= 0) {
      // Two digits (example: 12°C)
      drawDigit3x5(8,  baseY, d1, COL_TEMP);
      drawDigit3x5(12, baseY, d2, COL_TEMP);
      if (neg) { setXY(8, baseY - 1, COL_TEMP); setXY(9, baseY - 1, COL_TEMP); setXY(10, baseY - 1, COL_TEMP); }
      // Add 1-pixel gap before degree
      setXY(15, baseY, COL_TEMP); // ° at far right, with one pixel gap
    } else {
      // Single digit (example: 8°C)
      drawDigit3x5(10, baseY, d2, COL_TEMP);
      if (neg) { setXY(9, baseY - 1, COL_TEMP); setXY(10, baseY - 1, COL_TEMP); setXY(11, baseY - 1, COL_TEMP); }
      setXY(14, baseY, COL_TEMP); // ° symbol after one-pixel gap
    }
  }
}


// --------- HTTP /status ----------
void handleStatus(){
  DynamicJsonDocument doc(1024);
  struct tm tinfo; time_t now=time(nullptr);
  bool haveTime=getLocalTimeSafe(&tinfo);
  char iso[25]={0};
  if(haveTime){
    snprintf(iso,sizeof(iso),"%04d-%02d-%02d %02d:%02d:%02d",
      tinfo.tm_year+1900,tinfo.tm_mon+1,tinfo.tm_mday,tinfo.tm_hour,tinfo.tm_min,tinfo.tm_sec);
  }
  JsonObject timeObj=doc.createNestedObject("time");
  timeObj["HH"]=haveTime?tinfo.tm_hour:-1;
  timeObj["MM"]=haveTime?tinfo.tm_min:-1;
  timeObj["SS"]=haveTime?tinfo.tm_sec:-1;
  timeObj["iso"]=iso;

  if(isnan(g_tempC)) doc["temperature_c"]=nullptr; else doc["temperature_c"]=g_tempC;
  doc["weather_main"]=g_weatherMain;
  doc["ip"]=WiFi.localIP().toString();
  doc["tz"]=TZ_EUROPE_HELSINKI;
  doc["epoch"]=(uint32_t)now;

  String out; serializeJson(doc,out);
  server.send(200,"application/json",out);
}

// --------- Setup / Loop ----------
void setup(){
  Serial.begin(115200);
  strip.begin(); strip.setBrightness(BRIGHTNESS); strip.show();
  randomSeed((uint32_t)(micros() ^ (uint32_t)ESP.getEfuseMac()));

  ensureWiFi();
  initTime();
  fetchWeather();

  server.on("/status", HTTP_GET, handleStatus);
  server.on("/", HTTP_GET, [](){ server.send(200,"text/plain","OK. Try /status"); });
  server.begin();
  Serial.print("IP: "); Serial.println(WiFi.localIP());
}

void drawAll(){
  clearAll();

  // time / progress / weather
  struct tm t;
  if(getLocalTimeSafe(&t)){
    // (1) top time
    drawTopTime(t.tm_hour, t.tm_min);

    // (3) middle seconds field
    if(g_lastSecond > t.tm_sec){ // rollover 59->0
      clearSecondsField();
    }
    if(g_lastSecond != t.tm_sec){
      drawSecondsField(t.tm_sec); // add one pixel with new color
      g_lastSecond = t.tm_sec;
    }
  }

  // (5) bottom weather
  drawBottomWeather();

  strip.show();
}

void loop(){
  server.handleClient();
  unsigned long nowSec=millis()/1000;
  if(nowSec - g_lastWeatherFetch >= WEATHER_REFRESH_SEC) fetchWeather();
  drawAll();
  delay(50);
}
