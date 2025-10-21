/*
====================================================================================
 README – ESP32-C3 / M5Stamp-C3U Deep-Sleep Wake-on-LAN Controller
====================================================================================

 Project Overview
 ----------------
 This firmware turns an ESP32-C3 (tested with M5Stamp-C3U) into a low-power
 Wake-on-LAN controller that can power on a PC either through a relay pulse or a
 WOL magic packet. It also reports the PC’s ON/OFF state via an RGB LED.

 When idle, the ESP stays in deep sleep, consuming microamps.
 It wakes up only when the built-in button is pressed (GPIO9) or optionally on a
 timer. After performing its task, it automatically returns to deep sleep.

------------------------------------------------------------------------------------
 Hardware Summary
------------------------------------------------------------------------------------
 • MCU Board : ESP32-C3 or M5Stamp-C3U
 • Wi-Fi     : Uses existing 2.4 GHz LAN (works with Pi-hole DHCP)
 • Wake Btn  : GPIO9 (built-in) — deep-sleep wake source (EXT0, active-LOW)
 • Optional Btn : GPIO5 (external, pulled-up) — usable while awake
 • Relay OUT : GPIO6 → drives PC power relay (active HIGH pulse)
 • RGB LED   : GPIO2 (onboard SK6812) → GREEN = PC ON, OFF = PC OFF

------------------------------------------------------------------------------------
 Behavior Summary
------------------------------------------------------------------------------------
 1. **Deep Sleep Mode**
    - ESP32 is mostly off.
    - Wake sources:
        * Built-in button (GPIO9 → GND)
        * Optional timer (configurable in seconds)

 2. **On Wake**
    - Connects to Wi-Fi using stored SSID/PASS.
    - Checks if the PC is online:
        * ICMP ping via ESPPing library
        * If ping fails, tries TCP ports (3389, 445, 22, 80)
    - LED briefly shows result:
        * GREEN → PC is ON
        * OFF   → PC is OFF or not reachable
    - If the PC is OFF:
        * GPIO6 pulses HIGH for 800 ms → simulates power button press
        * Sends Wake-on-LAN magic packet to broadcast (255.255.255.255:9)
    - Remains awake for a short period (~1 s) so you can also press the external
      GPIO5 button to repeat the wake action if needed.
    - Turns Wi-Fi off, clears LED, and re-enters deep sleep.

 3. **Periodic Timer Wake (optional)**
    - If enabled (`TIMER_WAKE_SECS > 0`), the ESP wakes every N seconds,
      checks PC status, lights the LED briefly, and sleeps again.

------------------------------------------------------------------------------------
 Pinout & Connections
------------------------------------------------------------------------------------
   ┌───────────────────────────────┐
   │   M5Stamp-C3U (ESP32-C3)     │
   │                               │
   │   GPIO2  → SK6812 LED DI     │
   │   GPIO5  → External Button → GND (optional) │
   │   GPIO6  → Relay IN (active HIGH)           │
   │   GPIO9  → Built-in Button → GND (wake pin) │
   │   3V3/GND → Power/Relay module supply       │
   └───────────────────────────────┘

------------------------------------------------------------------------------------
 Configuration
------------------------------------------------------------------------------------
 • Edit the section **USER SETTINGS** below:
      WIFI_SSID / WIFI_PASSWORD
      PC_MAC_STR — your PC’s MAC address (for WOL)
      PC_IP_STR  — your PC’s static or reserved IP

 • Adjust optional parameters:
      RELAY_PULSE_MS     – length of relay pulse (ms)
      LED_VISIBLE_MS     – how long LED stays lit before sleeping
      TIMER_WAKE_SECS    – periodic wake interval (0 = disabled)

------------------------------------------------------------------------------------
 Library Requirements
------------------------------------------------------------------------------------
 • **ESPPing**           (for ping detection)
 • **Adafruit NeoPixel** (for SK6812 LED)
 • **WiFi / WiFiUDP**    (included with ESP32 core)

------------------------------------------------------------------------------------
 Typical Power Consumption
------------------------------------------------------------------------------------
 • Deep sleep: ~10–20 µA
 • Active Wi-Fi check: ~80–100 mA for ≈2–3 s

------------------------------------------------------------------------------------
 Tips
------------------------------------------------------------------------------------
 • Ensure your PC and ESP are on the same LAN/subnet (Ping + WoL broadcast work).
 • In Windows Firewall, enable “File and Printer Sharing → Echo Request (ICMPv4-In)”
   so ping can succeed, or rely on the TCP fallback check.
 • For reliable WoL, keep the PC BIOS and NIC configured to “Wake on Magic Packet”.

------------------------------------------------------------------------------------
 Author / Credits
------------------------------------------------------------------------------------
 • Firmware concept & consolidation by ChatGPT (ESP32 Projects Series)
 • Original base built from laptop-switch.ino project
 • Simplified & optimized for deep sleep operation

------------------------------------------------------------------------------------
 END OF README
====================================================================================
*/
/*
  ESP32-C3 / M5Stamp-C3U — Deep Sleep Wake-on-LAN + Relay + LED (NO HTTP/LOGS)

  - Wakes on: built-in button GPIO9 (active LOW, EXT0), and optional timer.
  - On wake:
      * Wi-Fi connect
      * If PC is OFF -> pulse GPIO6 (relay) + send WoL
      * Update LED: green if PC ONLINE, off if OFFLINE (visible briefly)
      * Wi-Fi off, LED off, back to deep sleep

  Hardware:
    Buttons: GPIO9 (M5Stamp built-in) for wake; GPIO5 still works *after* wake
    Relay  : GPIO6 (active HIGH pulse)
    LED    : SK6812 on GPIO2 (green when PC online)

  Serial baud: 115200
*/
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <ESPping.h>
#include <Adafruit_NeoPixel.h>

// ====== USER SETTINGS ======
const char* WIFI_SSID     = "SKYNET MESH NETWORK";
const char* WIFI_PASSWORD = "Evan.Sejnin.15";

// PC identifiers
const char* PC_MAC_STR = "08:8F:C3:17:8E:E2";     // <- change to your PC MAC
const char* PC_IP_STR  = "192.168.100.20";          // <- change to your PC IP

// ====== PINS ======
#define BUTTON_PIN     5     // external button
#define BUTTON2_PIN    9     // built-in button (M5Stamp)
#define RELAY_PIN      6     // relay output
#define LED_PIN        2     // SK6812 RGB LED
#define NUMPIXELS      1

// ====== TIMINGS ======
#define RELAY_PULSE_MS     800
#define BUTTON_DEBOUNCE_MS 40
#define LED_REFRESH_MS     5000

// ====== GLOBALS ======
WiFiUDP udp;
IPAddress pcIP;
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastCheckLED  = 0;
unsigned long lastBtnChange = 0;
bool lastBtn1 = HIGH;
bool lastBtn2 = HIGH;

// ---------- HELPERS ----------
bool parseMac(const char* macStr, uint8_t mac[6]) {
  int v[6];
  if (6 == sscanf(macStr, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5])) {
    for (int i = 0; i < 6; ++i) mac[i] = (uint8_t)v[i];
    return true;
  }
  return false;
}

void sendWoL(const uint8_t mac[6]) {
  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (int i = 0; i < 16; ++i) memcpy(&packet[6 + i * 6], mac, 6);

  static bool udpBegun = false;
  if (!udpBegun) { udp.begin(0); udpBegun = true; }

  udp.beginPacket(IPAddress(255,255,255,255), 9);
  udp.write(packet, sizeof(packet));
  udp.endPacket();

  Serial.println("[WOL] Magic packet sent");
}

// Try connecting to a TCP port if ICMP fails
bool pcIsOnlineTcp(uint16_t port = 3389, uint16_t timeout_ms = 400) {
  WiFiClient client;
  bool ok = client.connect(pcIP, port, timeout_ms);
  client.stop();
  return ok;
}

bool pcIsOnline() {
  if (!pcIP) return false;

  // Try ping first (ICMP)
  bool icmp = Ping.ping(pcIP, 1);
  if (icmp) return true;

  // Fallback to TCP port check (RDP 3389, SMB 445, etc.)
  const uint16_t ports[] = {3389, 445, 22, 80};
  for (uint16_t p : ports) {
    if (pcIsOnlineTcp(p)) return true;
  }
  return false;
}

void updateLed(bool on) {
  if (on) pixels.setPixelColor(0, pixels.Color(0, 255, 0));
  else    pixels.setPixelColor(0, 0);
  pixels.show();
}

void pulseRelayIfPcOff() {
  if (!pcIsOnline()) {
    digitalWrite(RELAY_PIN, HIGH);
    delay(RELAY_PULSE_MS);
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("[RELAY] Pulse triggered");
  } else {
    Serial.println("[RELAY] PC already ON, no pulse");
  }
}

void triggerWake() {
  Serial.println("[BTN] Wake trigger");
  pulseRelayIfPcOff();

  uint8_t mac[6];
  if (parseMac(PC_MAC_STR, mac)) sendWoL(mac);
  updateLed(pcIsOnline());
}

// ---------- BUTTON HANDLER ----------
void pollButtons() {
  bool b1 = digitalRead(BUTTON_PIN);
  bool b2 = digitalRead(BUTTON2_PIN);
  bool pressedNow = (b1 == LOW) || (b2 == LOW);
  bool pressedBefore = (lastBtn1 == LOW) || (lastBtn2 == LOW);

  if (millis() - lastBtnChange > BUTTON_DEBOUNCE_MS) {
    if (pressedNow && !pressedBefore) {
      triggerWake();
      lastBtnChange = millis();
    }
  }
  lastBtn1 = b1;
  lastBtn2 = b2;
}

// ---------- SETUP / LOOP ----------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("=== Boot ===");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pixels.begin();
  pixels.setBrightness(32);
  pixels.clear();
  pixels.show();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  Serial.printf("[WiFi] Connecting to %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    if (millis() - t0 > 20000) {
      Serial.println("\n[WiFi] Timeout, rebooting...");
      delay(2000);
      ESP.restart();
    }
  }
  Serial.printf("\n[WiFi] Connected, IP=%s\n", WiFi.localIP().toString().c_str());

  if (!pcIP.fromString(PC_IP_STR)) {
    Serial.println("[BOOT] Invalid PC IP string!");
  }

  // Initial LED
  bool on = pcIsOnline();
  updateLed(on);
  Serial.printf("[BOOT] PC %s\n", on ? "ONLINE" : "OFFLINE");

  lastCheckLED = millis();
}

void loop() {
  pollButtons();

  // periodic LED refresh
  if (millis() - lastCheckLED > LED_REFRESH_MS) {
    lastCheckLED = millis();
    bool on = pcIsOnline();
    updateLed(on);
    Serial.printf("[CHK] PC %s\n", on ? "ONLINE" : "OFFLINE");
  }

  delay(2);
}