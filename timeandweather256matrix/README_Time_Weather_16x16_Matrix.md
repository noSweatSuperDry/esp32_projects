
# Time & Weather 16×16 Matrix (ESP32-C3)

A compact Wi-Fi-connected **clock + weather display** built on a **16×16 WS2812 LED matrix** and an **ESP32-C3 (M5Stamp C3U)**.  
Displays current time, animated seconds, and weather + temperature, while serving real-time JSON data via HTTP.

---

## ✨ Features

- **Top (5×16)** — Digital clock (`HH MM`), no colon.
  - Hours → green  
  - Minutes → purple  
- **Middle (4×16)** — Seconds progress bar (fills one pixel per second, random color, resets every minute)
- **Bottom (5×16)** — Weather icon + temperature (with 1-pixel gap before °)
- **Wi-Fi + API:**
  - NTP for accurate time (auto time zone set to Europe/Helsinki)
  - OpenWeather API for temperature & condition (updated hourly)
  - Built-in HTTP server with `/status` JSON endpoint

---

## 🧰 Hardware Setup

| Component | Description |
|------------|-------------|
| **Controller** | M5Stamp ESP32-C3U |
| **LED Matrix** | 16×16 WS2812B (NeoPixel) |
| **Power** | 5 V, ≥2 A recommended |
| **Data Pin** | GPIO 7 (configurable) |

### Wiring

| ESP32 Pin | LED Matrix |
|------------|-------------|
| GPIO 7 | DIN |
| 5 V | 5 V |
| GND | GND |

> Ensure a **common GND** between the ESP32 and LED power source.  
> Start with `BRIGHTNESS = 32` to prevent power issues.

---

## 💾 Software Requirements

Install the following libraries in Arduino IDE (Library Manager):

- `Adafruit NeoPixel`
- `ArduinoJson`
- `WiFi`
- `HTTPClient`
- `WebServer`
- `time`

---

## ⚙️ Configuration

Open the sketch and edit the section at the top:

```cpp
#define WIFI_SSID        "YOUR_WIFI_SSID"
#define WIFI_PASS        "YOUR_WIFI_PASSWORD"
#define OPENWEATHER_API_KEY "YOUR_OPENWEATHER_KEY"

#define LATITUDE   60.1699
#define LONGITUDE  24.9384
#define DATA_PIN   7
#define BRIGHTNESS 32

static const char* TZ_EUROPE_HELSINKI = "EET-2EEST,M3.5.0/03,M10.5.0/04";
```

Optional static IP (e.g., `192.168.100.101`):

```cpp
IPAddress local(192,168,100,101), gateway(192,168,100,1), subnet(255,255,255,0), dns(1,1,1,1);
WiFi.config(local, gateway, subnet, dns);
WiFi.begin(WIFI_SSID, WIFI_PASS);
```

---

## 🧠 Display Layout

```
y=0 ┌─────────────────────────────┐   Top (5 px)   → Time (HH MM)
     │ 15 08                      │
y=4 └─────────────────────────────┘
y=5 ────────────── blank ───────────────
y=6 ┌─────────────────────────────┐   Middle (4 px) → Seconds color fill
     │ colored pixels (0–59)      │
y=9 └─────────────────────────────┘
y=10────────────── blank ──────────────
y=11┌─────────────────────────────┐   Bottom (5 px) → Weather icon + temperature
     │ ☁   8 °C                   │
y=15└─────────────────────────────┘
```

### Behavior

| Section | Description |
|----------|--------------|
| **Time** | Drawn with 3×5 font. No colon. |
| **Seconds** | Fills one pixel per second (random color). Clears at 60 s. |
| **Weather** | Simple 5×5 icon + integer temperature (°C). |
| **Network** | Auto Wi-Fi, hourly weather fetch, NTP time sync. |

---

## 🌐 HTTP Endpoint

| Endpoint | Description |
|-----------|-------------|
| `/` | Returns `"OK. Try /status"` |
| `/status` | Returns JSON containing time, temperature, weather, and colors |

### Example Response

```json
{
  "time": { "HH": 15, "MM": 8, "SS": 42, "iso": "2025-10-26 15:08:42" },
  "temperature_c": 8.0,
  "weather_main": "Clouds",
  "colors": {
    "hour": "#18FF00",
    "minute": "#8A00FF",
    "icon": "#12A7FF",
    "temperature": "#FF7F00"
  },
  "ip": "192.168.100.101",
  "tz": "EET-2EEST,M3.5.0/03,M10.5.0/04",
  "epoch": 1766796522
}
```

---

## 🔧 Build & Flash

1. **Board:** `ESP32C3 Dev Module` (or M5Stamp C3)
2. **Library install:** via Arduino Library Manager.
3. **Port:** Select your ESP32 serial port.
4. **Upload** the sketch.
5. **Open Serial Monitor** — IP address will be displayed once connected.

---

## 🧩 Customization

| Feature | Description |
|----------|-------------|
| **Font** | Replace 3×5 digit patterns. |
| **Brightness** | Change `BRIGHTNESS` (0–255). |
| **Units** | Change OpenWeather API to imperial for °F. |
| **Color themes** | Adjust `COL_HOUR`, `COL_MIN`, `COL_ICON`, `COL_TEMP`. |
| **Weather icons** | Modify or add new 5×5 icons. |
| **Seconds animation** | Change fill direction in `drawSecondsField()`. |

---

## 🩺 Troubleshooting

| Issue | Fix |
|--------|-----|
| `Adafruit_NeoPixel strip` compile error | Add missing semicolon after timezone line. |
| LEDs mirrored/upside-down | Edit `xy2i()` rotation logic. |
| No Wi-Fi/weather | Check credentials and API key. |
| Flickering / unstable | Use stable 5 V supply, lower brightness. |

---

## 🔐 Security

Do not expose `/status` to the public Internet.  
The OpenWeather API key is stored in firmware — keep sketches private.

---

## 📜 License

MIT-style: free to use and modify for personal or educational projects. Attribution appreciated.

---

## 🙌 Credits

Built with ❤️ using:
- ESP32 Arduino Core
- Adafruit NeoPixel
- ArduinoJson
- WebServer + HTTPClient

