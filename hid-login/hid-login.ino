#include <HijelHID_BLEKeyboard.h>

// MH-B IR sensor output pin
#define IR_SENSOR_PIN 4

// Most MH-B / IR obstacle modules output LOW when object is detected.
// If your sensor is opposite, change LOW to HIGH.
#define PRESENCE_ACTIVE_STATE LOW

const unsigned long debounceMs = 100;
const unsigned long wakeCooldownMs = 10000;

HijelHID_BLEKeyboard keyboard("Chair Wake Sensor", "ESP32-C6", 100);

bool lastRawPresence = false;
bool stablePresence = false;
bool previousStablePresence = false;

unsigned long lastDebounceTime = 0;
unsigned long lastWakeTime = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  Serial.println("Starting BLE HID keyboard...");
  keyboard.begin();

  Serial.println("Ready.");
  Serial.println("Pair 'Chair Wake Sensor' with your Mac over Bluetooth.");
}

void loop() {
  bool rawPresence = readPresence();

  if (rawPresence != lastRawPresence) {
    lastDebounceTime = millis();
    lastRawPresence = rawPresence;
  }

  if ((millis() - lastDebounceTime) > debounceMs) {
    stablePresence = rawPresence;
  }

  if (stablePresence && !previousStablePresence) {
    Serial.println("Chair presence detected.");

    if (keyboard.isConnected()) {
      unsigned long now = millis();

      if (now - lastWakeTime >= wakeCooldownMs) {
        wakeMac();
        lastWakeTime = now;
      } else {
        Serial.println("Cooldown active, wake skipped.");
      }
    } else {
      Serial.println("BLE keyboard is not connected.");
    }
  }

  if (!stablePresence && previousStablePresence) {
    Serial.println("Chair presence removed.");
  }

  previousStablePresence = stablePresence;

  delay(20);
}

bool readPresence() {
  int value = digitalRead(IR_SENSOR_PIN);
  return value == PRESENCE_ACTIVE_STATE;
}

void wakeMac() {
  Serial.println("Sending Shift key wake signal...");

  keyboard.press(KEY_LSHIFT);
  delay(80);
  keyboard.release(KEY_LSHIFT);
  keyboard.releaseAll();

  Serial.println("Wake signal sent.");
}