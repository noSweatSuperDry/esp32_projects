
# Time & Weather 16×16 LED Matrix (ESP32-C3)

A **Wi-Fi connected clock and weather display** for a **16×16 WS2812B LED matrix**, powered by an **ESP32-C3 (M5Stamp C3U)**.  
It shows current time, animated seconds fill, and temperature + weather icon — all in clean color-coded rows.  
It also includes a **web interface** for live configuration and a **JSON API** for status and debug control.

---

## 🧩 Display Layout

```
y=0 ┌─────────────────────────────┐   Top (5 px) → Time (HH MM)
     │ 15 08                      │
y=4 └─────────────────────────────┘
y=5 ───────────── blank ──────────────
y=6 ┌─────────────────────────────┐   4 rows → Seconds color fill
     │ 0–59 random-color pixels   │
y=9 └─────────────────────────────┘
y=10┌─────────────────────────────┐   Bottom (5 px) → Temp (left) + Icon (right)
     │ +08 °C         ☁           │
y=14└─────────────────────────────┘
x=0 ………………………………………… x=15 (16 columns)
```

✅ **Time (y=0–4)**: “HH MM”, no colon  
✅ **Seconds (y=6–9)**: fills with random colors each second, clears after 60  
✅ **Weather (y=10–14)**: temperature °C on left, icon on right  
✅ **All rows used** — no visual gaps

---

## ⚙️ Features

- **Accurate Time**
  - NTP-synced or manually set via web UI  
  - Local timezone support (Europe/Helsinki default)
- **Weather**
  - Uses [OpenWeatherMap](https://openweathermap.org/api)
  - Updates automatically every hour
- **Web Configuration UI**
  - Change between **NTP** and **Manual time**
  - Set OpenWeather API key
  - Update latitude/longitude
  - Trigger debug mode
- **JSON API** for programmatic access
- **Debug mode (6s demo)**
  - Shows “88 88”, cycles all icons and shows “+88°C”
  - Keeps filling seconds field in random colors

---

## 🧰 Hardware Setup

| Component | Description |
|------------|-------------|
| **Controller** | M5Stamp ESP32-C3U |
| **LED Matrix** | 16×16 WS2812B (NeoPixel) |
| **Power** | 5 V ≥ 2 A |
| **Data Pin** | GPIO 7 |

### Wiring

| ESP32-C3U | LED Matrix |
|------------|-------------|
| GPIO 7 | DIN |
| 5 V | 5 V |
| GND | GND |

> ⚠️ Make sure **ESP32 and matrix share GND**.  
> Start with **brightness = 32** (max = 255).

---

## 💾 Software Requirements

Install the following libraries (Arduino IDE → Library Manager):

- `Adafruit NeoPixel`
- `ArduinoJson`
- `WiFi`
- `HTTPClient`
- `WebServer`
- `time`

---

## ⚙️ Configuration

Edit these constants in the sketch before uploading:

```cpp
#define WIFI_SSID        "YOUR_WIFI_SSID"
#define WIFI_PASS        "YOUR_WIFI_PASSWORD"
#define DATA_PIN         7
#define BRIGHTNESS       32

String CFG_OPENWEATHER_API_KEY = "YOUR_OPENWEATHER_KEY";
float  CFG_LATITUDE  = 60.1699;
float  CFG_LONGITUDE = 24.9384;
```

Optional: set static IP (before `WiFi.begin()`):

```cpp
IPAddress local(192,168,100,101), gateway(192,168,100,1), subnet(255,255,255,0), dns(1,1,1,1);
WiFi.config(local, gateway, subnet, dns);
```

---

## 🌐 Web Interface

Once connected to Wi-Fi, open `http://<device-ip>/config`

You can:
- Switch between **NTP sync** or **Manual time**
- Set **OpenWeather API key**
- Update **latitude/longitude**
- Trigger **6-second debug demo**
- View live **JSON status** at `/status`

### Example `/status` Output

```json
{
  "time": { "HH": 12, "MM": 34, "SS": 45, "iso": "2025-10-27 12:34:45" },
  "temperature_c": 8.0,
  "weather_main": "Clouds",
  "config": {
    "time_mode": "ntp",
    "manual_iso": "2025-01-01T00:00:00",
    "lat": 60.1699,
    "lon": 24.9384
  },
  "ip": "192.168.100.101",
  "tz": "EET-2EEST,M3.5.0/03,M10.5.0/04",
  "epoch": 1766879685
}
```

---

## 🔧 JSON API Endpoints

| Endpoint | Method | Description |
|-----------|--------|-------------|
| `/api/set_time` | POST | `mode=ntp` or `mode=manual&iso=YYYY-MM-DDTHH:MM:SS` |
| `/api/set_weather` | POST | `apikey=YOUR_KEY` |
| `/api/set_location` | POST | `lat=60.17&lon=24.93` |
| `/api/debug` | POST | Triggers 6s debug demo |
| `/status` | GET | JSON system state |

---

## 🩺 Debug Demo

Press **“Start 6s Debug Demo”** in the web UI or:

```bash
curl -X POST http://<device-ip>/api/debug
```

Output:
- Time area shows `88 88`
- Middle fills with random colors
- Bottom cycles weather icons and shows `+88°C`
- Resets after 6 seconds

---

## 🧠 Behavior Summary

| Section | Rows | Function |
|----------|------|----------|
| Time | 0–4 | Shows `HH MM` (no colon) |
| Blank | 5 | Separator |
| Seconds field | 6–9 | 60-pixel random color fill, resets each minute |
| Weather | 10–14 | Temp (left) + icon (right) |
| Debug mode | — | Overrides display for 6 s demo |

---

## ⚡ Troubleshooting

| Problem | Possible Cause | Fix |
|----------|----------------|-----|
| No LEDs lit | Wrong pin or no common GND | Check `DATA_PIN` and wiring |
| Display mirrored/upside down | Matrix wiring order | Adjust mapping in `xy2i()` |
| Weather not updating | Invalid OpenWeather key | Verify in `/config` |
| Webpage blank | Try IP again or clear cache | Refresh browser |
| Power flicker | Overcurrent | Use stronger 5 V supply / reduce brightness |

---

## 🔒 Security Notes

- Do **not expose** `/config` or `/status` to the open internet.
- The OpenWeather API key is stored in device flash (NVS).

---

## 📜 License

Free for **personal and educational** use. Attribution appreciated.

---

## 🙌 Credits

- ESP32 Arduino Core  
- Adafruit NeoPixel  
- ArduinoJson  
- WebServer / HTTPClient  
- OpenWeather API
