/*
  USB Cable Tester — ESP32-C6 (built-in RGB)

  Uses the on-board single RGB LED exposed as RGB_BUILTIN and the helper:
    void rgbLedWrite(uint8_t pin, uint8_t r, uint8_t g, uint8_t b);

  Protocol (line-based, \n terminated):
    PING:<nonce>        -> PONG:<nonce>
    ECHO:<text>         -> ECHO:<text>
    LED:BLUE|GREEN|RED|OFF
    RUN                 -> RUNNING (also sets BLUE)

  Colors:
    Blue  = test running
    Green = pass
    Red   = fail
*/

#ifndef RGB_BUILTIN
// Most ESP32-C6 DevKitC-1 boards define RGB_BUILTIN in the core.
// If your core doesn't, set the pin here (GPIO 8 on ESP32-C6 DevKitC-1).
#define RGB_BUILTIN 8
#endif

// -------- LED helpers --------
static inline void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  rgbLedWrite(RGB_BUILTIN, r, g, b);
}
static inline void ledBlue()  { setRGB(0,   0, 255); }
static inline void ledGreen() { setRGB(0, 255,   0); }
static inline void ledRed()   { setRGB(255, 0,   0); }
static inline void ledOff()   { setRGB(0,   0,   0); }

// -------- Serial helpers --------
static String readLineTrimmed() {
  String s = Serial.readStringUntil('\n');
  s.trim();
  return s;
}

void setup() {
  // No need to "init" RGB_BUILTIN — the core handles it.
  ledOff();

  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0 < 2000)) { delay(10); }

  Serial.println("READY:USB_CABLE_TESTER");
  Serial.println("HINT: PING:<n>, ECHO:<text>, LED:BLUE|GREEN|RED|OFF, RUN");
}

void loop() {
  if (!Serial.available()) {
    delay(5);
    return;
  }

  String line = readLineTrimmed();

  // PING:<nonce> -> PONG:<nonce>
  if (line.startsWith("PING:")) {
    String nonce = line.substring(5);
    Serial.print("PONG:");
    Serial.println(nonce);
    return;
  }

  // ECHO:<text> -> ECHO:<text>
  if (line.startsWith("ECHO:")) {
    String payload = line.substring(5);
    Serial.print("ECHO:");
    Serial.println(payload);
    return;
  }

  // LED commands
  if (line.equalsIgnoreCase("LED:BLUE"))  { ledBlue();  Serial.println("OK"); return; }
  if (line.equalsIgnoreCase("LED:GREEN")) { ledGreen(); Serial.println("OK"); return; }
  if (line.equalsIgnoreCase("LED:RED"))   { ledRed();   Serial.println("OK"); return; }
  if (line.equalsIgnoreCase("LED:OFF"))   { ledOff();   Serial.println("OK"); return; }

  // RUN: set Blue + acknowledge
  if (line.equalsIgnoreCase("RUN")) {
    ledBlue();
    Serial.println("RUNNING");
    return;
  }

  // Unknown
  Serial.println("ERR:UNKNOWN_CMD");
}
