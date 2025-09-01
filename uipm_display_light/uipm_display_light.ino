#include <FastLED.h>

// --- LED setup ---
#define NUM_LEDS 1  // One LED per GPIO
#define LED_PIN_1 3
#define LED_PIN_2 4
#define LED_PIN_3 5
#define LED_PIN_4 6
#define LED_PIN_5 7

CRGB led1[NUM_LEDS];
CRGB led2[NUM_LEDS];
CRGB led3[NUM_LEDS];
CRGB led4[NUM_LEDS];
CRGB led5[NUM_LEDS];

// Global mapping pointer filled in setup()
CRGB** gLedMap = nullptr;

// --- Input pins ---
#define IN_PIN_1 8
#define IN_PIN_2 9
#define IN_PIN_3 10

// Configure these to match your hardware:
// If your UIPM wiring pulls the input LOW when active (switch to ground), set to 1.
// If your wiring is active-HIGH, set to 0.
#define INPUT_ACTIVE_LOW 1

// If the physical LED order is reversed (you're seeing left/right swapped), set to 1.
#define LED_ORDER_REVERSED 0

// Enable to print raw digitalRead() values in addition to logical state (helps debug wiring)
#define DEBUG_RAW_READS 0

// --- Variables to store state ---
int lastState[3] = { -1, -1, -1 };

// --- Forward declarations ---
void setLED(CRGB *led, CRGB color);
void setLEDAt(int idx, CRGB color);
void setAll(CRGB color);
void allOff();
void applyLEDLogic(int s[]);

// Startup blink using helper
void startupBlink() {
  for (int i = 0; i < 2; i++) {
    setAll(CRGB::Green);
    FastLED.show();
    delay(250);

    setAll(CRGB::Red);
    FastLED.show();
    delay(250);
  }
}

void setup() {
  Serial.begin(115200);

  // Add each LED strip/controller once
  FastLED.addLeds<WS2812, LED_PIN_1, GRB>(led1, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_2, GRB>(led2, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_3, GRB>(led3, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_4, GRB>(led4, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_5, GRB>(led5, NUM_LEDS);

#if LED_ORDER_REVERSED
  static CRGB* ledMapLocal[5] = { led5, led4, led3, led2, led1 };
#else
  static CRGB* ledMapLocal[5] = { led1, led2, led3, led4, led5 };
#endif

  // expose ledMap via global pointer so helpers can use it
  gLedMap = (CRGB**)ledMapLocal;

  // Configure inputs based on INPUT_ACTIVE_LOW setting.
#if INPUT_ACTIVE_LOW
  // Active = LOW, enable internal pull-ups (common UIPM wiring)
  pinMode(IN_PIN_1, INPUT_PULLUP);
  pinMode(IN_PIN_2, INPUT_PULLUP);
  pinMode(IN_PIN_3, INPUT_PULLUP);
#else
  // Active = HIGH. Prefer internal pull-down if available (ESP32). If your board
  // doesn't support INPUT_PULLDOWN, change these to plain INPUT and add external
  // pull-down resistors.
  pinMode(IN_PIN_1, INPUT_PULLDOWN);
  pinMode(IN_PIN_2, INPUT_PULLDOWN);
  pinMode(IN_PIN_3, INPUT_PULLDOWN);
#endif

  // Show blink sequence at startup
  startupBlink();

  // Start with all LEDs off
  allOff();
}

// Read input with simple debounce and return logical 1 when the input indicates "active".
// With INPUT_PULLUP we treat LOW as active (1).
int readInputLogical(int pin) {
  const int samples = 5;
  int sum = 0;
  for (int i = 0; i < samples; i++) {
  int raw = digitalRead(pin);
#if DEBUG_RAW_READS
  Serial.print("raw("); Serial.print(pin); Serial.print(")="); Serial.print(raw); Serial.print(" ");
#endif
  if (INPUT_ACTIVE_LOW) sum += (raw == LOW) ? 1 : 0;
  else sum += (raw == HIGH) ? 1 : 0;
    delay(5);
  }
  return (sum > samples / 2) ? 1 : 0;
}

void loop() {
  int s[3];
  s[0] = readInputLogical(IN_PIN_1);
  s[1] = readInputLogical(IN_PIN_2);
  s[2] = readInputLogical(IN_PIN_3);

  // Only update if state changed
  if (s[0] != lastState[0] || s[1] != lastState[1] || s[2] != lastState[2]) {
    Serial.print("GPIO State: ");
    Serial.print(s[0]);
    Serial.print(",");
    Serial.print(s[1]);
    Serial.print(",");
    Serial.println(s[2]);

    applyLEDLogic(s);

    // Save new state
    lastState[0] = s[0];
    lastState[1] = s[1];
    lastState[2] = s[2];
  }
}

// --- Helper: set LED color (direct pointer) ---
void setLED(CRGB *led, CRGB color) {
  led[0] = color;
}

// Set by logical index 0..4 using gLedMap
void setLEDAt(int idx, CRGB color) {
  if (gLedMap == nullptr || idx < 0 || idx > 4) return;
  setLED(gLedMap[idx], color);
}

// Set all five LEDs to the same color
void setAll(CRGB color) {
  for (int i = 0; i < 5; ++i) setLEDAt(i, color);
}

// --- Helper: turn off all LEDs ---
void allOff() {
  setAll(CRGB::Black);
  FastLED.show();
}

// --- Main LED logic ---
void applyLEDLogic(int s[]) {
  // UIPM states mapping (1..8). The original mapping used 1/0 values; here '1' means active.
  // Clear first
  setAll(CRGB::Black);

  if (s[0]==0 && s[1]==0 && s[2]==0) { // 1 -> all OFF
    // allOff already set
  }
  else if (s[0]==0 && s[1]==1 && s[2]==1) { // 2 -> all RED
    setAll(CRGB::Red);
  }
  else if (s[0]==1 && s[1]==0 && s[2]==1) { // 3 -> 1 Green, rest Red
    setLEDAt(0, CRGB::Green);
    setLEDAt(1, CRGB::Red);
    setLEDAt(2, CRGB::Red);
    setLEDAt(3, CRGB::Red);
    setLEDAt(4, CRGB::Red);
  }
  else if (s[0]==0 && s[1]==0 && s[2]==1) { // 4 -> 1 & 2 Green, rest Red
    setLEDAt(0, CRGB::Green);
    setLEDAt(1, CRGB::Green);
    setLEDAt(2, CRGB::Red);
    setLEDAt(3, CRGB::Red);
    setLEDAt(4, CRGB::Red);
  }
  else if (s[0]==1 && s[1]==1 && s[2]==0) { // 5 -> 1..3 Green, 4..5 Red
    setLEDAt(0, CRGB::Green);
    setLEDAt(1, CRGB::Green);
    setLEDAt(2, CRGB::Green);
    setLEDAt(3, CRGB::Red);
    setLEDAt(4, CRGB::Red);
  }
  else if (s[0]==0 && s[1]==1 && s[2]==0) { // 6 -> 1..4 Green, 5 Red
    setLEDAt(0, CRGB::Green);
    setLEDAt(1, CRGB::Green);
    setLEDAt(2, CRGB::Green);
    setLEDAt(3, CRGB::Green);
    setLEDAt(4, CRGB::Red);
  }
  else if (s[0]==1 && s[1]==0 && s[2]==0) { // 7 -> all Green
    setAll(CRGB::Green);
  }
  else if (s[0]==0 && s[1]==0 && s[2]==0) { // 8 - Blink Green/Red 2 times
    for (int i = 0; i < 2; i++) {
      setAll(CRGB::Green);
      FastLED.show();
      delay(250);

      setAll(CRGB::Red);
      FastLED.show();
      delay(250);
    }
    // After blink, keep them Red briefly then return to mapping (we'll set below)
  }

  FastLED.show();
}
