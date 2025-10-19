🖥️ ESP32-C3 Wake-on-LAN Controller (HTTP + Static IP + Web Config + Button Trigger)

A self-contained Wake-on-LAN (WoL) and Wi-Fi configuration server for the ESP32-C3.
It hosts a simple web UI for changing Wi-Fi credentials and target device MAC, supports static IP, Basic Auth for secure access, and allows you to wake your PC via:

Web interface → /wake button

Physical GPIO 5 button

Perfect for home-lab, automation, or remote PC control setups.

✨ Features
Feature	Description
🌐 HTTP Web UI	Simple, mobile-friendly config panel served from ESP32
🔐 Basic Auth	/settings & /factory protected (admin / snip33r)
⚙️ Wi-Fi Configurable	Change SSID, password, and laptop MAC from /settings
🖧 Static IP	Uses 192.168.100.201 with Pi-hole DNS and gateway 192.168.100.1
🧠 NVS Storage	Settings saved to flash (persistent across reboots)
📡 Wake-on-LAN	Sends standard WoL magic packet (UDP port 9)
🪫 AP Fallback	If Wi-Fi fails, starts hotspot ESP32C3-Setup (pass configureme)
🔘 Physical Button	GPIO 5, active-LOW (pull-up enabled), wakes target instantly
🧰 Factory Reset	/factory clears settings and reboots
🧩 Hardware Requirements

ESP32-C3 Dev Module (with onboard Wi-Fi)

Momentary push button

Optional: 3.3 V power supply or USB

Wiring
ESP32-C3 Pin	Component	Notes
GPIO 5	Push-button → GND	Internal pull-up used
3V3 / GND	Power	Standard power pins

No resistor needed (the code enables internal pull-up).

🌐 Web Endpoints
URL	Method	Auth	Purpose
/	GET	none	Status page + “Send Wake” button
/wake	POST	none	Send Wake-on-LAN magic packet
/settings	GET	✅ Basic Auth	Edit SSID, password, target MAC
/settings	POST	✅ Basic Auth	Save new settings, reboot
/factory	POST	✅ Basic Auth	Factory reset (clear NVS)
⚡️ Default Network Configuration
#define USE_STATIC_IP true
const IPAddress STATIC_IP (192,168,100,201);
const IPAddress DNS_IP    (192,168,100,100);  // Pi-hole
const IPAddress GATEWAY_IP(192,168,100,1);
const IPAddress SUBNET_IP (255,255,255,0);


If USE_STATIC_IP is set to false, DHCP is used instead.

🚀 Usage
1. First Boot (Setup Mode)

If the ESP32 can’t join Wi-Fi (no valid credentials):

It starts an access point:
SSID: ESP32C3-Setup
Password: configureme
IP: 192.168.4.1

Connect to that network and open:
👉 http://192.168.4.1/settings

Log in with:

Username: admin
Password: snip33r


Enter:

Home Wi-Fi SSID

Password

Laptop MAC address (e.g. 11:22:33:44:55:66)

Click Save & Reboot

2. Normal Operation

Once connected, open the static IP:

👉 http://192.168.100.201/

You’ll see:

Connection status

Target MAC

“Send Wake” button

Link to Settings (requires auth)

3. Sending Wake-on-LAN

Press “Send Wake” in the browser

Or physically press the GPIO 5 button

If your laptop supports Wake-on-LAN, it should power on from sleep/shutdown.

🧠 Laptop Configuration (Acer Aspire A514-54 example)

BIOS

Enable Wake on LAN (may require setting a supervisor password).

Windows Settings

Keep Fast Startup ON (needed for wake from shutdown on Acer models).

Network Adapter → Power Management → Allow this device to wake computer.

Advanced → Wake on Magic Packet = Enabled.

Connection Type

Prefer Ethernet (WoL over Wi-Fi is limited).

Leave on AC power for consistent wake behavior.

⚙️ How It Works

On boot, ESP32 tries saved Wi-Fi credentials.

If it fails, it launches AP setup mode.

When connected, it starts a lightweight HTTP server on port 80.

/wake constructs a magic packet:

FF FF FF FF FF FF + [MAC repeated 16 times]


and sends it via UDP broadcast on port 9.

Your laptop’s NIC (if WoL enabled) wakes the system.

🔧 Customization
Feature	Where to Change
Static IP / DNS	USE_STATIC_IP block
WoL Port	WOL_PORT constant
Auth Credentials	AUTH_USER, AUTH_PASS
Button GPIO	BUTTON_PIN constant
Debounce / long press	DEBOUNCE_MS, LONGPRESS_MS
🛠️ Dependencies

No external libraries required beyond ESP32 core:

WiFi.h

WebServer.h

WiFiUdp.h

Preferences.h

🧪 Tested On
Component	Version
Board	ESP32-C3 DevKitM-1
Framework	Arduino 2.0.14+ (ESP32 Core 3.x)
OS	Windows 11 + Acer Aspire A514-54
Network	Static IP, Pi-hole DNS
🔐 Security Notes

HTTP is plain-text; anyone on your LAN can access /wake.
Protect it behind a router firewall if accessible remotely.

/settings and /factory require Basic Auth.
Credentials are not encrypted over HTTP — use only on trusted LAN.

💡 Future Improvements

 HTTPS support (self-signed certs)

 WebSocket feedback or status API

 Support for multiple stored MACs

 mDNS (esp32.local)

 OTA updates via web UI

📸 Example Flow

Connect to Wi-Fi (or setup AP).

Visit http://192.168.100.201/settings.

Login → Update SSID, password, MAC.

Press Save & Reboot.

Visit http://192.168.100.201/ again → Send Wake.

Or press GPIO 5 button → instant WoL.

⚡️ License

MIT License
© 2025 Zahid Abdullah