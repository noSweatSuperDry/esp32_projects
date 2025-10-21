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