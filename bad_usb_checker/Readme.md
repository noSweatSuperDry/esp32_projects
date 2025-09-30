1) README.md (drop this in your repo root)
# USB Cable Tester — ESP32-C6 + Python Tk

A tiny, practical tool to sanity-check USB cables by talking to an ESP32-C6 over USB CDC.
The PC app shows **+5V / GND / TX / RX** as ✅/❌ and drives the board’s RGB LED:

- **Blue** = test running  
- **Green** = pass  
- **Red** = fail

## Repo layout



esp/usb_cable_tester.ino # ESP32-C6 firmware (Arduino)
pc/usb_cable_tester_gui.py # Tk GUI (auto-connect + Run)
requirements.txt # Python deps (pyserial)


## Hardware

**Board:** ESP32-C6-DevKitC-1 (any flash size).  
**On-board RGB LED:** WS2812, connected to **GPIO 8**.  
> If your LED doesn’t blink, see **Troubleshooting** below.

**Optional external WS2812:**
- DIN → GPIO 8
- 5V → 5V
- GND → GND  
Recommended: 330 Ω in series with DIN, 1000 µF across 5V/GND (near LED).

## Firmware (ESP32-C6 / Arduino)

1. Install **Arduino IDE 2.x** and **ESP32 core by Espressif**.
2. Library Manager → install **Adafruit NeoPixel**.
3. Open `esp/usb_cable_tester.ino`, select your **ESP32-C6** board, upload.
4. On boot, the board prints:


READY:USB_CABLE_TESTER
HINT: Send PING:<nonce>, LED:BLUE|GREEN|RED


### Serial mini-protocol

- `LED:BLUE|GREEN|RED` → sets LED, replies `OK`  
- `PING:<nonce>` → replies `PONG:<nonce>`  
- `RUN` → replies `RUNNING` (visual helper)

## PC App (Tk GUI)

### Install

```bash
# Windows
py -m pip install -r requirements.txt

# Linux / macOS
python3 -m pip install -r requirements.txt


If Tkinter is missing on Linux:
Ubuntu/Debian: sudo apt install python3-tk
Fedora: sudo dnf install python3-tkinter

Run
# Windows
py pc/usb_cable_tester_gui.py

# Linux / macOS
python3 pc/usb_cable_tester_gui.py


The app auto-scans serial ports (prefers Espressif VID 0x303A), reads the READY banner, and enables Run Test.

What the test checks

Sends LED:BLUE (Blue = running)

Sends PING:<random> and waits for PONG:<same>

Marks:

+5V ✅ if port opens (device enumerated & powered)

GND ✅ if communication is stable (banner/traffic read)

TX ✅ if write succeeded

RX ✅ if expected PONG received

All ✅ → LED:GREEN and Passed; otherwise LED:RED and Failed

Troubleshooting
LED not blinking on ESP32-C6-DevKitC-1

The on-board LED is addressable WS2812 on GPIO 8 (not a plain GPIO LED). Use a NeoPixel/WS2812 driver.

Make sure you installed Adafruit NeoPixel and called pixel.begin() and pixel.show().

If you still see nothing:

Try lowering brightness (setBrightness(16)).

Power the board via USB-C (either native USB or UART USB works).

Avoid long USB hubs/cables (WS2812 can be timing-sensitive).

References: Espressif docs and board guides confirm the WS2812 on GPIO 8. 
DFRobot Electronics
+4
Developer Portal
+4
circuitpython.org
+4

ModuleNotFoundError: No module named 'serial.tools'

Install pyserial for the Python you’re using:

# Windows
py -m pip install pyserial
# Linux/macOS
python3 -m pip install pyserial


Verify with:

python -m serial.tools.list_ports

Linux: permission denied on /dev/tty*

Add user to dialout (then log out/in):

sudo usermod -a -G dialout $USER

Packaging (optional)

Create a single-file executable:

# Windows
py -m pip install pyinstaller
py -m PyInstaller --onefile --noconsole pc/usb_cable_tester_gui.py

# macOS / Linux
python3 -m pip install pyinstaller
python3 -m PyInstaller --onefile pc/usb_cable_tester_gui.py