#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include <WebServer.h>
#include <Preferences.h>    // <-- NVS storage
#include <sys/time.h>       // <-- settimeofday()

// ================= USER CONFIG (initial defaults) =================
#define WIFI_SSID        "xxx"
#define WIFI_PASS        "xxx"

// Initial values — can be changed from /config and saved to NVS
String   CFG_OPENWEATHER_API_KEY = "xxx";
float    CFG_LATITUDE  = xxxf;
float    CFG_LONGITUDE = xxxf;
// Time mode: "ntp" or "manual"
String   CFG_TIME_MODE = "ntp";
String   CFG_MANUAL_ISO = "2025-01-01T00:00:00";

// Matrix
#define DATA_PIN   7
#define MATRIX_W   16
#define MATRIX_H   16
#define NUM_LEDS   (MATRIX_W * MATRIX_H)
#define BRIGHTNESS 32

// Weather refresh cadence
const unsigned long WEATHER_REFRESH_SEC = 3600;  // 1 hour

// Timezone (Helsinki)
static const char* TZ_EUROPE_HELSINKI = "EET-2EEST,M3.5.0/03,M10.5.0/04";
// =================================================================

Adafruit_NeoPixel strip(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
Preferences prefs;

// ------------ Helpers ------------
inline uint32_t C(uint8_t r, uint8_t g, uint8_t b){ return strip.Color(r,g,b); }
static void rgbFromColor(uint32_t col, uint8_t &r, uint8_t &g, uint8_t &b){ r=(col>>16)&0xFF; g=(col>>8)&0xFF; b=col&0xFF; }
static String hexColor(uint32_t col){ uint8_t r,g,b; rgbFromColor(col,r,g,b); char buf[8]; snprintf(buf,sizeof(buf),"#%02X%02X%02X",r,g,b); return String(buf); }
inline void clearAll(){ strip.clear(); }

// 180° rotation + serpentine (odd rows L->R, even rows R->L)
uint16_t xy2i(uint8_t x,uint8_t y){
  x = MATRIX_W - 1 - x;
  y = MATRIX_H - 1 - y;
  bool odd = y & 1;
  return odd ? (y*MATRIX_W + x) : (y*MATRIX_W + (MATRIX_W-1-x));
}
inline void setXY(uint8_t x,uint8_t y,uint32_t c){ if(x<MATRIX_W && y<MATRIX_H) strip.setPixelColor(xy2i(x,y), c); }

// ------------ Fonts 3x5 and 5x5 icons ------------
const uint8_t DIGIT_3x5[10][5] PROGMEM = {
  {0b111,0b101,0b101,0b101,0b111}, {0b010,0b110,0b010,0b010,0b111},
  {0b111,0b001,0b111,0b100,0b111}, {0b111,0b001,0b111,0b001,0b111},
  {0b101,0b101,0b111,0b001,0b001}, {0b111,0b100,0b111,0b001,0b111},
  {0b111,0b100,0b111,0b101,0b111}, {0b111,0b001,0b001,0b001,0b001},
  {0b111,0b101,0b111,0b101,0b111}, {0b111,0b101,0b111,0b001,0b111}
};
const uint8_t ICON_CLEAR_5x5[5]  PROGMEM = { 0b00100,0b01010,0b10001,0b01010,0b00100 };
const uint8_t ICON_CLOUDS_5x5[5] PROGMEM = { 0b00000,0b01110,0b11111,0b11111,0b01110 };
const uint8_t ICON_RAIN_5x5[5]   PROGMEM = { 0b00100,0b01110,0b11111,0b01010,0b00100 };
const uint8_t ICON_SNOW_5x5[5]   PROGMEM = { 0b00100,0b10101,0b01110,0b10101,0b00100 };
const uint8_t* ICONS_ARR[4] = { ICON_CLEAR_5x5, ICON_CLOUDS_5x5, ICON_RAIN_5x5, ICON_SNOW_5x5 };

// ------------ State ------------
float  g_tempC = NAN;
String g_weatherMain = "";
unsigned long g_lastWeatherFetch = 0;
int g_lastSecond = -1;

// Debug demo state
bool     g_debugActive = false;
uint32_t g_debugUntilMs = 0;
int      g_debugIconIdx = 0;   // 0..3, change each second

// ------------ Networking & time ------------
void initTimeNTP(){
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
  url += String(CFG_LATITUDE,4); url += "&lon="; url += String(CFG_LONGITUDE,4);
  url += "&units=metric&appid="; url += CFG_OPENWEATHER_API_KEY;
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

// ------------ Drawing primitives ------------
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

// ------------ UI sections (your layout) ------------
const uint32_t COL_HOUR = 0x0000FF00;
const uint32_t COL_MIN  = 0x009000FF;
const uint32_t COL_ICON = 0x0000A0FF;
const uint32_t COL_TEMP = 0x00FF8000;

void drawTopTime(uint8_t hh,uint8_t mm){
  // y=0..4; x: H1(0..2) gap3 H2(4..6) gap7 M1(9..11) gap12 M2(13..15 minus 1 col)
  drawDigit3x5(0, 0, (hh/10)%10, COL_HOUR);
  drawDigit3x5(4, 0,  hh%10,     COL_HOUR);
  drawDigit3x5(9, 0, (mm/10)%10, COL_MIN);
  drawDigit3x5(13,0,  mm%10,     COL_MIN);
}

void clearSecondsField(){ for(uint8_t y=6;y<=9;y++) for(uint8_t x=0;x<16;x++) setXY(x,y,0); }
void drawSecondsField(uint8_t sec){
  // sec 0..59 → row 0..3, col 0..15
  uint8_t i=sec; uint8_t row=i/16, col=i%16;
  uint8_t y=6+row, x=col;
  setXY(x,y, C(random(0,256), random(0,256), random(0,256)));
}

void drawBottomWeatherNormal(){
  // icon left
  const uint8_t* icon = ICON_CLOUDS_5x5;
  if      (g_weatherMain=="Clear")        icon = ICON_CLEAR_5x5;
  else if (g_weatherMain=="Clouds")       icon = ICON_CLOUDS_5x5;
  else if (g_weatherMain=="Rain")         icon = ICON_RAIN_5x5;
  else if (g_weatherMain=="Drizzle")      icon = ICON_RAIN_5x5;
  else if (g_weatherMain=="Thunderstorm") icon = ICON_RAIN_5x5;
  else if (g_weatherMain=="Snow")         icon = ICON_SNOW_5x5;

  drawIcon5x5(0,11,icon,COL_ICON);

  if(!isnan(g_tempC)){
    int t=(int)round(g_tempC); bool neg=t<0; int a=abs(t);
    int d1=(a>=10)?(a/10)%10:-1, d2=a%10; uint8_t baseY=11;
    if(d1>=0){
      drawDigit3x5(8,  baseY, d1, COL_TEMP);
      drawDigit3x5(12, baseY, d2, COL_TEMP);
      if(neg){ setXY(8,10,COL_TEMP); setXY(9,10,COL_TEMP); setXY(10,10,COL_TEMP); }
      setXY(15, baseY, COL_TEMP);  // ° with 1px gap
    }else{
      drawDigit3x5(10, baseY, d2, COL_TEMP);
      if(neg){ setXY(9,10,COL_TEMP); setXY(10,10,COL_TEMP); setXY(11,10,COL_TEMP); }
      setXY(14, baseY, COL_TEMP);
    }
  }
}

// ------------ Debug demo (6 seconds) ------------
void startDebugDemo(){
  g_debugActive = true;
  g_debugUntilMs = millis() + 6000;
  g_debugIconIdx = 0;
  g_lastSecond = -1; // reset second tracking
}
void drawBottomWeatherDebug(){
  drawIcon5x5(0,11, ICONS_ARR[g_debugIconIdx % 4], C(255,255,255));
  // +88°C
  drawDigit3x5(8, 11, 8, C(255,255,255));
  drawDigit3x5(12,11, 8, C(255,255,255));
  // plus sign and degree:
  setXY(7,12, C(255,255,255)); setXY(8,12, C(255,255,255)); setXY(9,12, C(255,255,255)); // tiny bar for +
  setXY(15,11, C(255,255,255));
}

// ------------ HTTP: /status (existing) ------------
void handleStatus(){
  DynamicJsonDocument doc(1024);
  struct tm tinfo; time_t now=time(nullptr); bool haveTime=getLocalTimeSafe(&tinfo);
  char iso[25]={0}; if(haveTime) snprintf(iso,sizeof(iso),"%04d-%02d-%02d %02d:%02d:%02d",
    tinfo.tm_year+1900,tinfo.tm_mon+1,tinfo.tm_mday,tinfo.tm_hour,tinfo.tm_min,tinfo.tm_sec);

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

  JsonObject cfg = doc.createNestedObject("config");
  cfg["time_mode"] = CFG_TIME_MODE;
  cfg["manual_iso"] = CFG_MANUAL_ISO;
  cfg["lat"] = CFG_LATITUDE;
  cfg["lon"] = CFG_LONGITUDE;

  String out; serializeJson(doc,out);
  server.send(200,"application/json",out);
}

// ------------ HTTP: simple config UI (/config) ------------
const char* FORM_HTML = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>Matrix Config</title>
<style>body{font-family:system-ui;margin:16px}label{display:block;margin-top:12px}input,select,button{font-size:16px;padding:6px} .row{margin:8px 0}</style>
</head><body>
<h2>Time & Weather Matrix</h2>
<form method="POST" action="/config/save">
  <div class="row"><label>Time mode
    <select name="time_mode">
      <option value="ntp">NTP</option>
      <option value="manual">Manual</option>
    </select>
  </label></div>
  <div class="row"><label>Manual time (ISO) <small>YYYY-MM-DDTHH:MM:SS</small>
    <input name="manual_iso" placeholder="2025-01-01T12:34:56">
  </label></div>
  <div class="row"><label>OpenWeather API Key
    <input name="apikey" placeholder="xxxxxxxxxxxxxxxxxxxx">
  </label></div>
  <div class="row"><label>Latitude <input name="lat" type="number" step="0.0001"></label></div>
  <div class="row"><label>Longitude <input name="lon" type="number" step="0.0001"></label></div>
  <div class="row"><button type="submit">Save</button></div>
</form>
<form method="POST" action="/api/debug"><button>Start 6s Debug Demo</button></form>
<hr>
<p><a href="/status">View JSON status</a></p>
<script>
// pre-fill values
document.querySelector('select[name=time_mode]').value = "%TIME_MODE%";
document.querySelector('input[name=manual_iso]').value = "%MANUAL_ISO%";
document.querySelector('input[name=apikey]').value = "%APIKEY%";
document.querySelector('input[name=lat]').value = "%LAT%";
document.querySelector('input[name=lon]').value = "%LON%";
</script>
</body></html>
)HTML";

String renderConfigPage(){
  String html = FORM_HTML;
  html.replace("%TIME_MODE%",  CFG_TIME_MODE);
  html.replace("%MANUAL_ISO%", CFG_MANUAL_ISO);
  html.replace("%APIKEY%",     CFG_OPENWEATHER_API_KEY);
  html.replace("%LAT%",        String(CFG_LATITUDE,4));
  html.replace("%LON%",        String(CFG_LONGITUDE,4));
  return html;
}
void handleConfigPage(){ server.send(200, "text/html", renderConfigPage()); }

void savePrefs(){
  prefs.begin("twcfg", false);
  prefs.putString("time_mode", CFG_TIME_MODE);
  prefs.putString("manual_iso", CFG_MANUAL_ISO);
  prefs.putString("apikey", CFG_OPENWEATHER_API_KEY);
  prefs.putFloat("lat", CFG_LATITUDE);
  prefs.putFloat("lon", CFG_LONGITUDE);
  prefs.end();
}
void loadPrefs(){
  prefs.begin("twcfg", true);
  CFG_TIME_MODE = prefs.getString("time_mode", CFG_TIME_MODE);
  CFG_MANUAL_ISO = prefs.getString("manual_iso", CFG_MANUAL_ISO);
  CFG_OPENWEATHER_API_KEY = prefs.getString("apikey", CFG_OPENWEATHER_API_KEY);
  CFG_LATITUDE = prefs.getFloat("lat", CFG_LATITUDE);
  CFG_LONGITUDE = prefs.getFloat("lon", CFG_LONGITUDE);
  prefs.end();
}

// Parse ISO "YYYY-MM-DDTHH:MM:SS" as local time and set RTC
bool setManualTimeFromISO(const String& iso){
  if(iso.length() < 19) return false;
  struct tm t = {};
  t.tm_year = iso.substring(0,4).toInt() - 1900;
  t.tm_mon  = iso.substring(5,7).toInt() - 1;
  t.tm_mday = iso.substring(8,10).toInt();
  t.tm_hour = iso.substring(11,13).toInt();
  t.tm_min  = iso.substring(14,16).toInt();
  t.tm_sec  = iso.substring(17,19).toInt();
  time_t epoch = mktime(&t); // interprets as local time based on TZ
  if(epoch <= 0) return false;
  struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
  settimeofday(&tv, nullptr);
  return true;
}

// POST /config/save  (from HTML form)
void handleConfigSave(){
  if(server.hasArg("time_mode"))  CFG_TIME_MODE = server.arg("time_mode");
  if(server.hasArg("manual_iso")) CFG_MANUAL_ISO = server.arg("manual_iso");
  if(server.hasArg("apikey"))     CFG_OPENWEATHER_API_KEY = server.arg("apikey");
  if(server.hasArg("lat"))        CFG_LATITUDE  = server.arg("lat").toFloat();
  if(server.hasArg("lon"))        CFG_LONGITUDE = server.arg("lon").toFloat();
  savePrefs();

  // Apply time mode immediately
  if(CFG_TIME_MODE == "ntp"){
    initTimeNTP(); // next loop will use NTP time
  } else {
    setManualTimeFromISO(CFG_MANUAL_ISO);
  }

  server.sendHeader("Location", "/config");
  server.send(302, "text/plain", "Saved");
}

// JSON APIs
void handleApiSetTime(){
  String mode = server.arg("mode");
  String iso  = server.arg("iso");
  if(mode.length()) CFG_TIME_MODE = mode;
  if(iso.length())  CFG_MANUAL_ISO = iso;

  bool ok = true;
  if(CFG_TIME_MODE == "ntp"){
    initTimeNTP();
  } else if(CFG_TIME_MODE == "manual"){
    ok = setManualTimeFromISO(CFG_MANUAL_ISO);
  }
  savePrefs();
  DynamicJsonDocument doc(256); doc["ok"]=ok; doc["mode"]=CFG_TIME_MODE; doc["iso"]=CFG_MANUAL_ISO;
  String out; serializeJson(doc,out); server.send(200,"application/json",out);
}
void handleApiSetWeather(){
  if(server.hasArg("apikey")) CFG_OPENWEATHER_API_KEY = server.arg("apikey");
  savePrefs();
  server.send(200,"application/json", "{\"ok\":true}");
}
void handleApiSetLocation(){
  if(server.hasArg("lat")) CFG_LATITUDE  = server.arg("lat").toFloat();
  if(server.hasArg("lon")) CFG_LONGITUDE = server.arg("lon").toFloat();
  savePrefs();
  server.send(200,"application/json", "{\"ok\":true}");
}
void handleApiDebug(){
  startDebugDemo();
  server.send(200,"application/json","{\"ok\":true,\"debug\":true}");
}

// ------------ Setup / Loop ------------
void setup(){
  Serial.begin(115200);
  strip.begin(); strip.setBrightness(BRIGHTNESS); strip.show();
  randomSeed((uint32_t)(micros() ^ (uint32_t)ESP.getEfuseMac()));

  ensureWiFi();

  loadPrefs();
  setenv("TZ", TZ_EUROPE_HELSINKI, 1);
  tzset();

  if(CFG_TIME_MODE == "ntp") initTimeNTP();
  else                       setManualTimeFromISO(CFG_MANUAL_ISO);

  fetchWeather();

  // Routes
  server.on("/", HTTP_GET, [](){ server.send(200,"text/plain","OK. Try /config or /status"); });
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/config", HTTP_GET, handleConfigPage);
  server.on("/config/save", HTTP_POST, handleConfigSave);
  server.on("/api/set_time", HTTP_POST, handleApiSetTime);
  server.on("/api/set_weather", HTTP_POST, handleApiSetWeather);
  server.on("/api/set_location", HTTP_POST, handleApiSetLocation);
  server.on("/api/debug", HTTP_POST, handleApiDebug);
  server.begin();

  Serial.print("IP: "); Serial.println(WiFi.localIP());
}

void drawAll(){
  clearAll();

  // Time / Seconds / Weather, OR Debug demo override
  struct tm t;
  bool haveTime = getLocalTimeSafe(&t);
  int curSec = haveTime ? t.tm_sec : 0;

  bool inDebug = g_debugActive && (millis() < g_debugUntilMs);
  if(inDebug){
    // Top time: “88 88”
    drawDigit3x5(0,  0, 8, C(255,255,255));
    drawDigit3x5(4,  0, 8, C(255,255,255));
    drawDigit3x5(9,  0, 8, C(255,255,255));
    drawDigit3x5(13, 0, 8, C(255,255,255));

    // Middle seconds field: keep coloring one random pixel per second
    if(g_lastSecond < 0 || (curSec != g_lastSecond)){
      // On each new second also advance icon index for bottom
      g_debugIconIdx = (g_debugIconIdx + 1) % 4;
      // Use second modulo 60 to paint pixel, clear at rollover
      if(g_lastSecond > curSec) clearSecondsField();
      drawSecondsField(curSec % 60);
      g_lastSecond = curSec;
    }

    // Bottom: cycle icons, +88°C
    drawBottomWeatherDebug();

    // End debug after timeout
    if(millis() >= g_debugUntilMs){ g_debugActive=false; g_lastSecond=-1; }
    strip.show();
    return;
  }

  // Normal UI
  if(haveTime){
    drawTopTime(t.tm_hour, t.tm_min);

    if(g_lastSecond > curSec) clearSecondsField();         // rollover 59->0
    if(g_lastSecond != curSec){ drawSecondsField(curSec); g_lastSecond = curSec; }
  }
  drawBottomWeatherNormal();
  strip.show();
}

void loop(){
  server.handleClient();
  unsigned long nowSec = millis()/1000;
  if(nowSec - g_lastWeatherFetch >= WEATHER_REFRESH_SEC) fetchWeather();
  drawAll();
  delay(50);
}
