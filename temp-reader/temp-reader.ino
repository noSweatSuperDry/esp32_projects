#include <WiFiS3.h>
#include "Arduino_LED_Matrix.h"
#include <math.h>  // roundf

ArduinoLEDMatrix matrix;

// ====== Wi-Fi CONFIG ======
const char* WIFI_SSID = "SKYNET PRIME NETWORK";
const char* WIFI_PASS = "EvanMs2013-2025!#";

// 1 = use static IP (Pi-hole DNS etc.); 0 = use DHCP
#define USE_STATIC_IP 1

#if USE_STATIC_IP
  const IPAddress STATIC_IP (192,168,100,150);
  const IPAddress DNS_IP    (192,168,100,100);  // Pi-hole
  const IPAddress GATEWAY_IP(192,168,100,1);    // Router
  const IPAddress SUBNET_IP (255,255,255,0);
#endif

// ====== OpenWeatherMap (HTTPS) ======
const char* HOST = "api.openweathermap.org";
const int   PORT = 443;  // HTTPS
// Joensuu ~ 62.60 N, 29.76 E
const char* PATH = "/data/2.5/weather?lat=62.60&lon=29.76&units=metric&appid=81cb256407545916590ca9b97a0919bc";

// ====== LED Matrix framebuffer (12x8) ======
uint8_t frame[8][12];

// 3x5 font digits
const uint8_t FONT3x5[][3] PROGMEM = {
  {0b11111,0b10001,0b11111}, // 0
  {0b00000,0b00000,0b11111}, // 1
  {0b11101,0b10101,0b10111}, // 2
  {0b10101,0b10101,0b11111}, // 3
  {0b00111,0b00100,0b11111}, // 4
  {0b10111,0b10101,0b11101}, // 5
  {0b11111,0b10101,0b11101}, // 6
  {0b00001,0b00001,0b11111}, // 7
  {0b11111,0b10101,0b11111}, // 8
  {0b10111,0b10101,0b11111}, // 9
};

// Symbols (3 cols each)
const uint8_t SYM_MINUS[3] = {0b00100,0b00100,0b00100}; // '-'
const uint8_t SYM_DEG[3]   = {0b00111,0b00101,0b00111}; // '°'
const uint8_t SYM_C[3]     = {0b11111,0b10001,0b10001}; // 'C'

// ---------- Matrix helpers ----------
void clearFrame() {
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 12; c++)
      frame[r][c] = 0;
}
void setPixel(int x, int y, bool on) {
  if (x < 0 || x >= 12 || y < 0 || y >= 8) return;
  frame[y][x] = on ? 1 : 0;
}
void blit3x5(const uint8_t glyph[3], int x, int y) {
  for (int col = 0; col < 3; col++) {
    uint8_t colBits = glyph[col];
    for (int row = 0; row < 5; row++) {
      bool on = (colBits >> row) & 0x01;
      setPixel(x + col, y + row, on);
    }
  }
}
void drawDigit(char d, int x, int y) {
  if (d < '0' || d > '9') return;
  uint8_t buf[3];
  for (int i = 0; i < 3; i++) buf[i] = pgm_read_byte(&FONT3x5[d - '0'][i]);
  blit3x5(buf, x, y);
}
void drawMinus(int x, int y) { blit3x5(SYM_MINUS, x, y + 2); }
void drawDeg(int x, int y)   { blit3x5(SYM_DEG,   x, y); }
void drawC(int x, int y)     { blit3x5(SYM_C,     x, y); }

// ---------- Brightness (non-blocking PWM) ----------
uint8_t BRIGHTNESS = 8;           // 1..10 (10 brightest)
const uint16_t PWM_PERIOD_MS = 20;
bool lastShownOn = false;

void refreshDisplay() {
  uint16_t onTime = (BRIGHTNESS >= 10) ? PWM_PERIOD_MS
                                       : (uint16_t)((PWM_PERIOD_MS * BRIGHTNESS) / 10);
  unsigned long phase = millis() % PWM_PERIOD_MS;
  bool showOn = (BRIGHTNESS >= 10) ? true : (phase < onTime);

  if (showOn != lastShownOn) {
    if (showOn) {
      matrix.renderBitmap(frame, 8, 12);
    } else {
      static uint8_t blank[8][12] = {0};
      matrix.renderBitmap(blank, 8, 12);
    }
    lastShownOn = showOn;
  }
}

// ---------- Render temperature into framebuffer ----------
void renderTemp(int tempC) {
  clearFrame();
  bool neg = tempC < 0;
  int t = abs(tempC);
  int tens = t / 10;
  int ones = t % 10;

  int width = (neg ? 1 : 0) + (tens ? 3 : 0) + 3 + 1 + 3 + 3; // [-]TT O [ ] ° C
  int startX = (12 - width) / 2;
  int y = 1;
  int x = startX;

  if (neg) { drawMinus(x, y); x += 1; }
  if (tens) { drawDigit('0' + tens, x, y); x += 3; }
  drawDigit('0' + ones, x, y); x += 3;
  x += 1;                     // spacer
  drawDeg(x, y - 1); x += 3;
  drawC(x, y);
}

// ---------- Networking: OpenWeatherMap (HTTPS) ----------
float fetchTemperatureJoensuu() {
  WiFiSSLClient client;  // HTTPS client for api.openweathermap.org
  if (!client.connect(HOST, PORT)) {
    Serial.println("Connect to API failed");
    return NAN;
  }

  // Use HTTP/1.0 to avoid chunked encoding
  client.print(String("GET ") + PATH + " HTTP/1.0\r\n");
  client.print(String("Host: ") + HOST + "\r\n");
  client.print("User-Agent: UNO-R4\r\n");
  client.print("Connection: close\r\n\r\n");

  // Skip headers
  String line;
  while (client.connected()) {
    line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  // Read body
  String body;
  while (client.available()) {
    body += client.readString();
  }
  client.stop();

  // Look for: "main":{"temp":<value>
  int mainIdx = body.indexOf("\"main\"");
  if (mainIdx < 0) return NAN;
  int tempKey = body.indexOf("\"temp\"", mainIdx);
  if (tempKey < 0) return NAN;
  int colon   = body.indexOf(':', tempKey);
  if (colon < 0) return NAN;

  int start = colon + 1;
  while (start < (int)body.length() && (body[start] == ' ' || body[start] == '\t')) start++;
  int end = start;
  while (end < (int)body.length()) {
    char ch = body[end];
    if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '+') end++;
    else break;
  }
  return body.substring(start, end).toFloat(); // °C thanks to units=metric
}

// ---------- Wi-Fi ----------
void connectWiFi() {
  Serial.print("Connecting to "); Serial.println(WIFI_SSID);

  WiFi.disconnect();
  delay(300);

#if USE_STATIC_IP
  WiFi.config(STATIC_IP, DNS_IP, GATEWAY_IP, SUBNET_IP);
#endif

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Wait for association
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000UL) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Link failed (check SSID/pass/signal).");
    return;
  }

#if USE_STATIC_IP
  Serial.println("Using STATIC IP config.");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
#else
  // If DHCP, ensure non-zero IP
  IPAddress got = WiFi.localIP();
  t0 = millis();
  while (got == IPAddress(0,0,0,0) && millis() - t0 < 10000UL) {
    delay(250);
    got = WiFi.localIP();
  }
  if (got == IPAddress(0,0,0,0)) {
    Serial.println("Got link but no DHCP (IP 0.0.0.0).");
  }
  Serial.print("IP: "); Serial.println(WiFi.localIP());
#endif

  Serial.print("RSSI: "); Serial.println(WiFi.RSSI());
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(115200);
  delay(1000);

  matrix.begin();
  connectWiFi();

  // Test pattern so you immediately see the matrix
  renderTemp(88); // shows "88°C" until first API fetch
}

unsigned long lastFetch = 0;
const unsigned long FETCH_EVERY_MS = 5UL * 60UL * 1000UL; // 5 minutes

void loop() {
  // Keep matrix refreshed (non-blocking)
  refreshDisplay();

  // Reconnect if needed
  if (WiFi.status() != WL_CONNECTED
#if !USE_STATIC_IP
      || WiFi.localIP() == IPAddress(0,0,0,0)
#endif
  ) {
    connectWiFi();
  }

  // Periodic temperature fetch
  if (millis() - lastFetch > FETCH_EVERY_MS || lastFetch == 0) {
    float temp = fetchTemperatureJoensuu();
    if (!isnan(temp)) {
      int ti = (int)roundf(temp);
      Serial.print("Joensuu temp: "); Serial.print(ti); Serial.println(" °C");
      renderTemp(ti);
    } else {
      Serial.println("Failed to read temperature; showing placeholder.");
      renderTemp(0);
    }
    lastFetch = millis();
  }
}
