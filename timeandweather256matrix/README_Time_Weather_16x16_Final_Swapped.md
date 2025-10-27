
# Time & Weather 16×16 LED Matrix (ESP32-C3)

A **Wi-Fi connected clock and weather display** for a **16×16 WS2812B LED matrix**, powered by an **ESP32-C3 (M5Stamp C3U)**.  
It shows current time, animated seconds fill, and weather icon + temperature — all in clean color-coded rows.  
It also includes a **web interface** for configuration and a **JSON API** for status and debug control.

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
y=10┌─────────────────────────────┐   Bottom (5 px) → Icon (left) + Temp (right)
     │ ☁          +08 °C          │
y=14└─────────────────────────────┘
x=0 ………………………………………… x=15 (16 columns)
```

✅ **Time (y=0–4)**: “HH MM”, no colon  
✅ **Seconds (y=6–9)**: fills with random colors each second, clears after 60  
✅ **Weather (y=10–14)**: icon left, temperature °C right  
✅ **All rows used** — no visual gaps

---

## ⚙️ Features

- **Accurate Time**
  - NTP-synced or manually set via web UI  
  - Local timezone (Europe/Helsinki default)
- **Weather**
  - Uses [OpenWeatherMap](https://openweathermap.org/api)
  - Updates every hour
- **Web Configuration UI**
  - Switch between **NTP** and **Manual time**
  - Update OpenWeather API key, latitude, longitude
  - Start debug demo
- **Color System**
  - Hour, minute, icon, and temperature colors auto-change every hour  
  - New random palette at boot
- **Debug mode (6s demo)**
  - Displays `88 88` time, cycles all icons, shows `+88°C`
  - Resets automatically after 6 seconds

---

## 🧰 Hardware Setup

| Component | Description |
|------------|-------------|
| **Controller** | M5Stamp ESP32-C3U |
| **LED Matrix** | 16×16 WS2812B (NeoPixel) |
| **Power** | 5 V ≥ 2 A |
| **Data Pin** | GPIO 7 |

### Wiring

| ESP32-C3U | LED Matrix |
|------------|-------------|
| GPIO 7 | DIN |
| 5 V | 5 V |
| GND | GND |

> ⚠️ Ensure common GND between ESP32 and LED matrix.  
> Start with **brightness = 32** (max = 255).

---

## 💾 Software Requirements

Install via Arduino IDE → Library Manager:

- `Adafruit NeoPixel`
- `ArduinoJson`
- `WiFi`
- `HTTPClient`
- `WebServer`
- `time`

---

## ⚙️ Configuration

In the sketch, edit these before upload:

```cpp
#define WIFI_SSID        "YOUR_WIFI_SSID"
#define WIFI_PASS        "YOUR_WIFI_PASSWORD"
#define DATA_PIN         7
#define BRIGHTNESS       32

String CFG_OPENWEATHER_API_KEY = "YOUR_OPENWEATHER_KEY";
float  CFG_LATITUDE  = 60.1699;
float  CFG_LONGITUDE = 24.9384;
```

Optional static IP:

```cpp
IPAddress local(192,168,100,101), gateway(192,168,100,1), subnet(255,255,255,0), dns(1,1,1,1);
WiFi.config(local, gateway, subnet, dns);
```

---

## 🌐 Web Interface

Open `http://<device-ip>/config`

You can:
- Toggle between NTP and manual time
- Edit API key / latitude / longitude
- Start debug demo
- View JSON status

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
  "colors": {
    "hour": "#A1FF00",
    "minute": "#00FFD8",
    "icon": "#0047FF",
    "temperature": "#FF006E"
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
| `/api/set_weather` | POST | Update OpenWeather API key |
| `/api/set_location` | POST | Update latitude & longitude |
| `/api/debug` | POST | Start 6s debug demo |
| `/status` | GET | Return JSON system state |

---

## 🩺 Debug Demo

Start via web or:
```bash
curl -X POST http://<device-ip>/api/debug
```

Displays:
- `88 88` as time
- Color fill animation in seconds area
- Cycles icons and shows `+88°C`
- Ends automatically after 6 seconds

---

## 🧠 Behavior Summary

| Section | Rows | Function |
|----------|------|----------|
| Time | 0–4 | Displays HH MM (no colon) |
| Blank | 5 | Spacer |
| Seconds Field | 6–9 | 60-pixel random fill, resets every minute |
| Weather | 10–14 | Icon left, temperature right |
| Debug | — | Overrides all for 6 s demo |

---

## ⚡ Troubleshooting

| Problem | Cause | Fix |
|----------|--------|-----|
| LEDs off | Wrong pin / no GND | Check `DATA_PIN`, GND shared |
| Mirrored display | Serpentine direction | Adjust `xy2i()` |
| No weather | Invalid API key | Update in `/config` |
| Webpage blank | Cached old UI | Refresh browser |
| Power flicker | Weak 5 V | Lower brightness or use higher current |

---

## 🔒 Security Notes

- Do **not** expose `/config` or `/status` publicly.  
- API key is stored in device NVS.

---

## 📜 License

Open source under MIT-style license. Free for personal or educational use.

---

## 🙌 Credits

- ESP32 Arduino Core  
- Adafruit NeoPixel  
- ArduinoJson  
- WebServer / HTTPClient  
- OpenWeather API
