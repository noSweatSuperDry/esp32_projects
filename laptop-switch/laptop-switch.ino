/*
  ESP32-C3 / M5Stamp-C3U — Wake-on-LAN + Status + Relay + LED + In-Browser Logs (FIXED)

  Features:
  - "/"      : Status + Wake button
  - "/wake"  : POST -> if PC is OFF -> relay pulse (GPIO6), then WoL
  - "/status": JSON {"online":true/false}
  - "/diag"  : Quick diagnostics + 1-shot ping
  - "/log"   : Live logs (HTML, auto-refresh)
  - "/logs"  : Raw logs (text/plain), last N lines

  Hardware:
  - Buttons: GPIO5 (external to GND + PULLUP), GPIO9 (M5Stamp built-in)
  - Relay:   GPIO6 (HIGH pulse when PC OFF)
  - LED:     SK6812 on GPIO2 (green when PC online, off otherwise)

  Serial baud: 115200
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <ESPping.h>            // <-- per your setup
#include <Adafruit_NeoPixel.h>

// ====== USER SETTINGS ======
const char* WIFI_SSID     = "SKYNET MESH NETWORK";
const char* WIFI_PASSWORD = "Evan.Sejnin.15";

// PC identifiers
const char* PC_MAC_STR = "08:8F:C3:17:8E:E2";     // <- change to your PC MAC
const char* PC_IP_STR  = "192.168.100.20";          // <- change to your PC IP

// ====== PINS (ESP32-C3 / M5Stamp-C3U) ======
#define BUTTON_PIN     5     // external button (to GND, uses pull-up)
#define BUTTON2_PIN    9     // built-in M5Stamp-C3U button
#define RELAY_PIN      6     // relay IN (active HIGH pulse)
#define LED_PIN        2     // SK6812 DI on M5Stamp-C3U
#define NUMPIXELS      1

// ====== TIMINGS ======
#define RELAY_PULSE_MS     800
#define BUTTON_DEBOUNCE_MS 40
#define LED_REFRESH_MS     5000

// ====== LOG BUFFER (ring) ======
#define LOG_LINES      300      // keep last 300 lines
#define LOG_LINE_MAX   220      // max chars per line stored
char _logBuf[LOG_LINES][LOG_LINE_MAX];
uint16_t _logHead = 0;          // next write index
uint16_t _logCount = 0;         // number of valid lines in buffer

// --- logging helpers (now support both const char* and String) ---
void _appendLogLine(const char* s) {
  size_t n = strnlen(s, LOG_LINE_MAX - 1);
  memcpy(_logBuf[_logHead], s, n);
  _logBuf[_logHead][n] = 0;
  _logHead = (_logHead + 1) % LOG_LINES;
  if (_logCount < LOG_LINES) _logCount++;
}
void _appendLogLine(const String& s) { _appendLogLine(s.c_str()); }

void _logln(const char* s) { Serial.println(s); _appendLogLine(s); }
void _logln(const String& s) { Serial.println(s); _appendLogLine(s); }

void _log(const char* s) { Serial.print(s); _appendLogLine(s); }
void _log(const String& s) { Serial.print(s); _appendLogLine(s); }

void _logf(const char* fmt, ...) {
  char tmp[LOG_LINE_MAX];
  va_list ap; va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  Serial.print(tmp);
  _appendLogLine(tmp);
}

#define LOGF(...)  _logf(__VA_ARGS__)
#define LOGLN(s)   _logln(s)
#define LOG(s)     _log(s)

// ====== Globals ======
WebServer server(80);
WiFiUDP udp;
IPAddress pcIP;

Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastCheckLED = 0;
unsigned long lastBtnChange = 0;
bool lastBtnLevel  = true;   // pull-up inputs -> HIGH = idle
bool lastBtn2Level = true;

// ---------- Utils ----------
bool parseMac(const char* macStr, uint8_t mac[6]) {
  int v[6];
  if (6 == sscanf(macStr, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5])) {
    for (int i = 0; i < 6; ++i) mac[i] = (uint8_t)v[i];
    return true;
  }
  return false;
}

void printMac(const uint8_t mac[6]) {
  LOGF("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void sendWoL(const uint8_t mac[6]) {
  // Build magic packet
  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (int i = 0; i < 16; ++i) memcpy(&packet[6 + i * 6], mac, 6);

  LOG("[WOL] Sending magic packet to broadcast: MAC=");
  printMac(mac);
  LOGF(", size=%d bytes\n", (int)sizeof(packet));

  // Keep UDP active; begin once on an ephemeral port
  static bool udpBegun = false;
  if (!udpBegun) {
    bool b = udp.begin(0);             // no .localPort() here (not available on your core)
    LOGF("[UDP] begin(0) -> %s\n", b ? "OK" : "FAIL");
    udpBegun = b;
  }

  bool ok = udp.beginPacket(IPAddress(255,255,255,255), 9); // broadcast
  LOGF("[WOL] beginPacket -> %s\n", ok ? "OK" : "FAIL");
  size_t w = udp.write(packet, sizeof(packet));
  LOGF("[WOL] write -> %u bytes\n", (unsigned)w);
  ok = udp.endPacket();
  LOGF("[WOL] endPacket -> %s\n", ok ? "OK" : "FAIL");
}

bool pcIsOnlineVerbose(unsigned attempts = 1) {
  if (!pcIP) {
    LOGLN("[PING] ERROR: pcIP invalid or unset");
    return false;
  }
  LOG("[PING] Pinging ");
  LOG(pcIP.toString());   // now accepted by LOG(String)
  LOGF(" x%u ...\n", attempts);

  bool any = false;
  for (unsigned i = 0; i < attempts; ++i) {
    bool r = Ping.ping(pcIP, 1);   // 1 probe per attempt
    LOGF("[PING] try %u -> %s (avg=%ld ms)\n", i+1, r ? "SUCCESS" : "FAIL", Ping.averageTime());
    any |= r;
  }
  LOGF("[PING] result OVERALL: %s\n", any ? "ONLINE" : "OFFLINE");
  return any;
}

bool pcIsOnline() {
  return pcIsOnlineVerbose(1);
}

void ledShowOnline(bool online) {
  if (online) {
    pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // green
  } else {
    pixels.setPixelColor(0, 0);                       // off
  }
  pixels.show();
  LOGF("[LED] %s\n", online ? "GREEN (ONLINE)" : "OFF (OFFLINE)");
}

void updateLedByStatus() {
  bool on = pcIsOnline();
  ledShowOnline(on);
}

void pulseRelayIfPcIsOff() {
  bool on = pcIsOnlineVerbose(1);
  if (!on) {
    LOGF("[RELAY] PC OFF -> pulse HIGH on GPIO %d for %u ms\n", RELAY_PIN, (unsigned)RELAY_PULSE_MS);
    digitalWrite(RELAY_PIN, HIGH);
    delay(RELAY_PULSE_MS);
    digitalWrite(RELAY_PIN, LOW);
    LOGLN("[RELAY] Done. GPIO set LOW");
  } else {
    LOGF("[RELAY] PC seems ON -> not pulsing. GPIO %d kept LOW\n", RELAY_PIN);
    digitalWrite(RELAY_PIN, LOW);
  }
}

void triggerWake() {
  LOGLN("[TRIGGER] Wake requested");
  pulseRelayIfPcIsOff();

  uint8_t mac[6];
  if (parseMac(PC_MAC_STR, mac)) {
    LOG("[TRIGGER] WoL for MAC=");
    printMac(mac);
    LOGLN("");
    sendWoL(mac);
  } else {
    LOG("[TRIGGER] ERROR: Failed to parse PC_MAC_STR '");
    LOG(PC_MAC_STR);
    LOGLN("'");
  }

  updateLedByStatus();
}

// ---------- HTTP ----------
String htmlIndex() {
  bool online = pcIsOnline();
  String s;
  s  = F("<!doctype html><html><head><meta charset='utf-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>ESP32C3 Wake</title>"
         "<style>"
         "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;margin:2rem;}"
         ".card{max-width:520px;padding:16px;border:1px solid #ddd;border-radius:12px;}"
         "button{font-size:16px;padding:10px 18px;border-radius:10px;border:0;background:#111;color:#fff;}"
         ".ok{color:#0a0;}.bad{color:#a00;}.mono{font-family:ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;}"
         "a{color:#06c;text-decoration:none} a:hover{text-decoration:underline}"
         "</style></head><body><div class='card'>");
  s += F("<h2>PC Status</h2><p>Status: <b class='");
  s += online ? "ok'>ON" : "bad'>OFF";
  s += F("</b></p><form method='POST' action='/wake'>"
         "<button type='submit'>Wake</button></form>"
         "<p style='margin-top:1rem;color:#555;font-size:13px'>"
         "Buttons GPIO5 & GPIO9 trigger wake. Relay on GPIO6 pulses only if PC is OFF.</p>"
         "<p class='mono'>PC IP: ");
  s += pcIP.toString();
  s += F("<br>Local IP: ");
  s += WiFi.localIP().toString();
  s += F("<br>GW: ");
  s += WiFi.gatewayIP().toString();
  s += F("<br>RSSI: ");
  s += String(WiFi.RSSI());
  s += F(" dBm</p>"
         "<p><a href='/diag'>Diagnostics</a> · <a href='/log'>Logs</a></p>"
         "</div></body></html>");
  return s;
}

void handleRoot() {
  LOGLN("[HTTP] GET /");
  server.send(200, "text/html", htmlIndex());
  LOGLN("[HTTP] 200 OK (/)");
}

void handleStatus() {
  LOGLN("[HTTP] GET /status");
  bool online = pcIsOnline();
  String json = String("{\"online\":") + (online ? "true" : "false") + "}";
  server.send(200, "application/json", json);
  LOGF("[HTTP] 200 OK (/status) -> %s\n", json.c_str());
}

void handlePostWake() {
  LOGLN("[HTTP] POST /wake");
  triggerWake();
  server.send(200, "text/plain", "OK");
  LOGLN("[HTTP] 200 OK (/wake)");
}

void handleDiag() {
  LOGLN("[HTTP] GET /diag");
  String s;
  s  = F("<!doctype html><html><head><meta charset='utf-8'><title>Diag</title>"
         "<style>body{font-family:system-ui;margin:2rem;} code{background:#f5f5f5;padding:2px 6px;border-radius:6px;}"
         "pre{background:#fafafa;border:1px solid #eee;padding:12px;border-radius:10px;overflow:auto;}</style>"
         "</head><body><h2>Diagnostics</h2><ul>");
  s += F("<li>WiFi SSID: <code>");
  s += WIFI_SSID;
  s += F("</code></li><li>Local IP: <code>");
  s += WiFi.localIP().toString();
  s += F("</code></li><li>Gateway: <code>");
  s += WiFi.gatewayIP().toString();
  s += F("</code></li><li>RSSI: <code>");
  s += String(WiFi.RSSI());
  s += F(" dBm</code></li><li>PC IP: <code>");
  s += pcIP.toString();
  s += F("</code></li><li>PC MAC: <code>");
  s += PC_MAC_STR;
  s += F("</code></li></ul><h3>One-shot Ping</h3><pre>");

  bool pong = pcIsOnlineVerbose(1);
  s += (pong ? "ONLINE\n" : "OFFLINE\n");

  s += F("</pre><p><a href='/'>Back</a></p></body></html>");
  server.send(200, "text/html", s);
  LOGLN("[HTTP] 200 OK (/diag)");
}

// -------- LOG PAGES --------
void handleLogPage() {
  LOGLN("[HTTP] GET /log");
  String html;
  html  = F("<!doctype html><html><head><meta charset='utf-8'><title>Logs</title>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<style>body{font-family:system-ui;margin:1rem;} #out{white-space:pre; font-family:ui-monospace,Menlo,Consolas,monospace;"
            "border:1px solid #ddd; padding:12px; border-radius:10px; max-width:100%; height:65vh; overflow:auto; background:#fafafa;}</style>"
            "</head><body><h2>Device Logs</h2>"
            "<div id='out'>(loading...)</div>"
            "<p><a href='/'>Home</a> · <a href='/diag'>Diagnostics</a></p>"
            "<script>"
            "async function pull(){"
              "try{const r=await fetch('/logs'); const t=await r.text();"
              "const out=document.getElementById('out'); out.textContent=t; out.scrollTop=out.scrollHeight;}catch(e){console.error(e);}"
            "}"
            "pull(); setInterval(pull,1000);"
            "</script></body></html>");
  server.send(200, "text/html", html);
  LOGLN("[HTTP] 200 OK (/log)");
}

void handleLogsRaw() {
  // Optional: ?n=150 to limit
  int n = server.hasArg("n") ? server.arg("n").toInt() : LOG_LINES;
  if (n < 1) n = 1;
  if (n > LOG_LINES) n = LOG_LINES;

  String s;
  // Compute starting index: oldest among the last n lines
  uint16_t use = (_logCount < (uint16_t)n) ? _logCount : (uint16_t)n;
  uint16_t start = (_logHead + LOG_LINES - _logCount) % LOG_LINES;
  uint16_t skip = (_logCount > (uint16_t)n) ? (_logCount - (uint16_t)n) : 0;
  start = (start + skip) % LOG_LINES;

  for (uint16_t i = 0; i < use; ++i) {
    uint16_t idx = (start + i) % LOG_LINES;
    s += _logBuf[idx];
    s += '\n';
  }
  server.send(200, "text/plain; charset=utf-8", s);
}

// ---------- Buttons ----------
void pollButtons() {
  bool b1 = digitalRead(BUTTON_PIN);    // HIGH idle
  bool b2 = digitalRead(BUTTON2_PIN);   // HIGH idle
  bool pressedNow = (b1 == LOW) || (b2 == LOW);
  bool pressedBefore = (lastBtnLevel == LOW) || (lastBtn2Level == LOW);

  unsigned long now = millis();
  if (now - lastBtnChange > BUTTON_DEBOUNCE_MS) {
    if (pressedNow && !pressedBefore) {
      LOGF("[BTN] PRESS (b1=%d,b2=%d) @ %lu ms\n", (int)b1, (int)b2, now);
      triggerWake();
      lastBtnChange = now;
    }
    if (!pressedNow && pressedBefore) {
      LOGF("[BTN] RELEASE (b1=%d,b2=%d) @ %lu ms\n", (int)b1, (int)b2, now);
      lastBtnChange = now;
    }
  }
  lastBtnLevel  = b1;
  lastBtn2Level = b2;
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);
  delay(200);
  LOGLN("\n=== Boot ===");

  // Pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // LED
  pixels.begin();
  pixels.setBrightness(32);
  pixels.clear();
  pixels.show();
  LOGLN("[LED] Initialized (brightness 32)");

  // Wi-Fi
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // keep radio awake for consistent UDP/ARP
  LOG("[WiFi] Connecting to SSID: ");
  LOGLN(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(350);
    LOG(".");
    if (millis() - t0 > 20000) { // 20s timeout
      LOGLN("\n[WiFi] TIMEOUT connecting. Rebooting in 3s...");
      delay(3000);
      ESP.restart();
    }
  }
  LOGLN("\n[WiFi] Connected!");
  LOG("[WiFi] IP: "); LOGLN(WiFi.localIP().toString());
  LOG("[WiFi] GW: "); LOGLN(WiFi.gatewayIP().toString());
  LOG("[WiFi] RSSI: "); LOGF("%d dBm\n", WiFi.RSSI());

  // Parse PC IP
  if (!pcIP.fromString(PC_IP_STR)) {
    LOG("[BOOT] ERROR: Invalid PC_IP_STR: ");
    LOGLN(PC_IP_STR);
  } else {
    LOG("[BOOT] PC_IP: "); LOGLN(pcIP.toString());
  }

  LOGLN("[UDP] Ready for broadcast packets");

  // HTTP
  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/status", HTTP_GET,  handleStatus);
  server.on("/wake",   HTTP_POST, handlePostWake);
  server.on("/diag",   HTTP_GET,  handleDiag);
  server.on("/log",    HTTP_GET,  handleLogPage);
  server.on("/logs",   HTTP_GET,  handleLogsRaw);
  server.onNotFound([](){
    LOG("[HTTP] 404 Not Found: ");
    LOGLN(server.uri());
    server.send(404, "text/plain", "Not Found");
  });
  server.begin();
  LOGLN("[HTTP] Server started on port 80");

  // Initial LED update
  updateLedByStatus();
  lastCheckLED = millis();
}

void loop() {
  server.handleClient();
  pollButtons();

  // periodic LED refresh from ping
  if (millis() - lastCheckLED > LED_REFRESH_MS) {
    lastCheckLED = millis();
    LOGLN("[TASK] Periodic status refresh -> LED update");
    updateLedByStatus();
  }

  delay(2);
}
