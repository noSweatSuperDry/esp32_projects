/*
  ESP32-C3 HTTP Config + Wake-on-LAN Server
  - HTTP (no TLS)
  - Endpoints:
      GET  /            -> status + Wake button
      GET  /settings    -> settings form (Basic Auth)
      POST /settings    -> save Wi-Fi + MAC to NVS, reboot (Basic Auth)
      POST /wake        -> send WoL magic packet
      POST /factory     -> clear NVS, reboot (Basic Auth)
  - Static IP support (set USE_STATIC_IP)
  - Fallback AP "ESP32C3-Setup" if STA connect fails
  - Physical button on GPIO 5 triggers WoL

  Board: ESP32C3 Dev Module
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <Preferences.h>

// ====== CONFIG ======
#define USE_STATIC_IP  true   // Set false to use DHCP

#if USE_STATIC_IP
const IPAddress STATIC_IP (192,168,100,201);
const IPAddress DNS_IP    (192,168,100,100);  // Pi-hole
const IPAddress GATEWAY_IP(192,168,100,1);    // Router
const IPAddress SUBNET_IP (255,255,255,0);
#endif

static const uint16_t WOL_PORT = 9;
static const char* AP_SSID = "Acer Aspire Button";
static const char* AP_PASS = "snip33r";

// Basic Auth
static const char* AUTH_USER = "admin";
static const char* AUTH_PASS = "snip33r";

// Physical button (active-LOW with pull-up)
static const int BUTTON_PIN = 5;
static const unsigned long DEBOUNCE_MS   = 40;
static const unsigned long LONGPRESS_MS  = 600; // (reserved if you want)
// ====== GLOBALS ======
Preferences prefs;
WebServer server(80);
WiFiUDP udp;

struct Settings {
  String ssid;
  String pass;
  String mac; // "11:22:33:44:55:66"
} settings;

// Button state
int lastStable = HIGH;     // INPUT_PULLUP idle = HIGH
int lastRead   = HIGH;
bool pressed   = false;
unsigned long lastChange = 0;
unsigned long pressStart = 0;

// ====== HELPERS ======
bool parseMac(const String &macStr, uint8_t out[6]) {
  int v[6];
  if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x", &v[0],&v[1],&v[2],&v[3],&v[4],&v[5]) != 6) return false;
  for (int i=0;i<6;i++) out[i] = (uint8_t)v[i];
  return true;
}

IPAddress broadcastIP() {
  IPAddress ip = WiFi.localIP(), mask = WiFi.subnetMask(), bc;
  for (int i=0;i<4;i++) bc[i] = (ip[i] & mask[i]) | (~mask[i] & 0xFF);
  return bc;
}

void sendWoL() {
  uint8_t mac[6];
  if (!parseMac(settings.mac, mac)) {
    Serial.println("[WOL] Invalid MAC, abort");
    return;
  }
  uint8_t packet[6 + 16*6];
  memset(packet, 0xFF, 6);
  for (int i=0;i<16;i++) memcpy(packet + 6 + i*6, mac, 6);
  IPAddress bcast = broadcastIP();
  udp.beginPacket(bcast, WOL_PORT);
  udp.write(packet, sizeof(packet));
  udp.endPacket();
  Serial.printf("[WOL] Sent to %s:%u\n", bcast.toString().c_str(), WOL_PORT);
}

// ====== NVS ======
void loadSettings() {
  prefs.begin("cfg", true);
  settings.ssid = prefs.getString("ssid", "");
  settings.pass = prefs.getString("pass", "");
  settings.mac  = prefs.getString("mac",  "00:00:00:00:00:00");
  prefs.end();
}
void saveSettings() {
  prefs.begin("cfg", false);
  prefs.putString("ssid", settings.ssid);
  prefs.putString("pass", settings.pass);
  prefs.putString("mac",  settings.mac);
  prefs.end();
}
void clearSettings() {
  prefs.begin("cfg", false);
  prefs.clear();
  prefs.end();
}

// ====== HTML UI ======
const char PAGE_HEADER[] PROGMEM =
"<!doctype html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32C3 Control</title>"
"<style>body{font-family:system-ui;margin:2rem;max-width:720px}"
"input,button{font-size:1rem;padding:.6rem;border-radius:.5rem;border:1px solid #ccc}"
"form>div{margin:.6rem 0}.card{padding:1rem;border:1px solid #ddd;border-radius:.75rem;margin:1rem 0}"
".row{display:flex;gap:.5rem;flex-wrap:wrap}label{display:block;margin-bottom:.3rem;font-weight:600}"
".btn{cursor:pointer}.btn-primary{background:#eee}.ok{color:#0a0}.warn{color:#a60}.err{color:#c00}</style></head><body>";

const char PAGE_FOOTER[] PROGMEM = "</body></html>";

String htmlIndex() {
  String s = FPSTR(PAGE_HEADER);
  s += "<h1>ESP32-C3: Wake-on-LAN</h1><div class='card'><b>Status</b><br>";
  if (WiFi.isConnected()) {
    s += "Mode: STA (connected)<br>IP: <b>http://" + WiFi.localIP().toString() + "</b><br>";
    s += "SSID: " + WiFi.SSID() + "<br>";
  } else if (WiFi.getMode() & WIFI_MODE_AP) {
    s += "Mode: AP (setup)<br>SSID: <b>" + String(AP_SSID) + "</b><br>";
  } else {
    s += "<span class='warn'>Wi-Fi not connected</span><br>";
  }
  s += "Target MAC: <b>" + settings.mac + "</b></div>";
  s += "<div class='row'>"
       "<form action='/wake' method='post'><button class='btn btn-primary'>Send Wake</button></form>"
       "<a href='/settings'><button class='btn' type='button'>Open Settings</button></a>"
       "</div>";
  s += FPSTR(PAGE_FOOTER);
  return s;
}

String htmlSettings(const String &msg="") {
  String s = FPSTR(PAGE_HEADER);
  s += "<h1>Settings</h1>";
  if (msg.length()) s += "<p class='ok'>" + msg + "</p>";
  s += "<form method='post' action='/settings'>"
       "<div><label>Wi-Fi SSID</label><input name='ssid' value='" + settings.ssid + "' required></div>"
       "<div><label>Wi-Fi Password</label><input name='pass' value='" + settings.pass + "'></div>"
       "<div><label>Laptop MAC (11:22:33:44:55:66)</label>"
       "<input name='mac' value='" + settings.mac + "' pattern='^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$' required></div>"
       "<div class='row'><button class='btn btn-primary' type='submit'>Save & Reboot</button>"
       "<a href='/'><button class='btn' type='button'>Back</button></a></div>"
       "</form>"
       "<div class='card'><b>Danger Zone</b>"
       "<form method='post' action='/factory'><button class='btn' style='background:#fee'>Factory Reset</button></form>"
       "</div>";
  s += FPSTR(PAGE_FOOTER);
  return s;
}

// ====== AUTH HELPERS ======
bool requireAuth() {
  if (!server.authenticate(AUTH_USER, AUTH_PASS)) {
    server.requestAuthentication(BASIC_AUTH, "ESP32C3 Settings", "Authentication required");
    return false;
  }
  return true;
}

// ====== HTTP HANDLERS ======
void handleRoot()             { server.send(200, "text/html", htmlIndex()); }

void handleGetSettings() {
  if (!requireAuth()) return;
  server.send(200, "text/html", htmlSettings());
}

void handlePostSettings() {
  if (!requireAuth()) return;
  String ssid = server.hasArg("ssid") ? server.arg("ssid") : "";
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  String mac  = server.hasArg("mac")  ? server.arg("mac")  : "";
  uint8_t tmp[6];
  if (ssid.isEmpty() || !parseMac(mac, tmp)) {
    server.send(400, "text/html", "<h3>Invalid input</h3><a href='/settings'>Back</a>");
    return;
  }
  settings.ssid = ssid;
  settings.pass = pass;
  settings.mac  = mac;
  saveSettings();
  server.send(200, "text/html", "<h3>Saved. Rebooting…</h3><script>setTimeout(()=>location.href='/',3000);</script>");
  delay(500);
  ESP.restart();
}

void handlePostWake() {
  // keep /wake open (no auth) so your button/form works easily
  sendWoL();
  server.send(200, "text/html", "<p class='ok'>Magic packet sent.</p><a href='/'>Back</a>");
}

void handlePostFactory() {
  if (!requireAuth()) return;
  clearSettings();
  server.send(200, "text/html", "<h3>Factory reset done. Rebooting…</h3>");
  delay(500);
  ESP.restart();
}

// ====== WIFI CONNECT ======
bool tryConnectSTA(const String &ssid, const String &pass, uint32_t msTimeout=15000) {
  if (ssid.isEmpty()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

#if USE_STATIC_IP
  if (!WiFi.config(STATIC_IP, GATEWAY_IP, SUBNET_IP, DNS_IP)) {
    Serial.println("[WiFi] Failed to configure static IP!");
  } else {
    Serial.printf("[WiFi] Using static IP %s\n", STATIC_IP.toString().c_str());
  }
#else
  Serial.println("[WiFi] Using DHCP");
#endif

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[WiFi] Connecting to \"%s\"", ssid.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < msTimeout) {
    Serial.print(".");
    delay(250);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println("[WiFi] Connect timeout");
  return false;
}

void startAP() {
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[AP] %s (pwd: %s) IP: %s %s\n", AP_SSID, AP_PASS, ip.toString().c_str(), ok?"":"(FAILED)");
}

// ====== BUTTON (GPIO 5) ======
void pollButton() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastRead) {
    lastRead = reading;
    lastChange = millis();
  }
  // debounce window passed?
  if (millis() - lastChange > DEBOUNCE_MS) {
    if (reading != lastStable) {
      lastStable = reading;
      if (lastStable == LOW) {           // pressed
        pressed = true;
        pressStart = millis();
      } else if (pressed && lastStable == HIGH) { // released
        unsigned long dur = millis() - pressStart;
        pressed = false;
        // short/long both send WoL; you can branch by dur if needed
        sendWoL();
      }
    }
  }
}

// ====== SETUP & LOOP ======
void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  loadSettings();

  if (!tryConnectSTA(settings.ssid, settings.pass)) {
    startAP();
  }

  udp.begin(WOL_PORT);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_GET, handleGetSettings);
  server.on("/settings", HTTP_POST, handlePostSettings);
  server.on("/wake", HTTP_POST, handlePostWake);
  server.on("/factory", HTTP_POST, handlePostFactory);
  server.begin();

  Serial.println("[HTTP] Server started on port 80");
}

void loop() {
  server.handleClient();
  pollButton();
  delay(2);
}
