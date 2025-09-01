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

// --- Input pins ---
#define IN_PIN_1 8
#define IN_PIN_2 9
#define IN_PIN_3 10

// --- Variables to store state ---
int lastState[3] = { -1, -1, -1 };

// --- Forward declarations ---
void setLED(CRGB *led, CRGB color);
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

  // Configure inputs with internal pullups.
  // ASSUMPTION: UIPM wiring uses switches to ground (active LOW). If your hardware uses
  // pull-down resistors or active HIGH signals, change INPUT_PULLUP and invert read logic.
  pinMode(IN_PIN_1, INPUT_PULLUP);
  pinMode(IN_PIN_2, INPUT_PULLUP);
  pinMode(IN_PIN_3, INPUT_PULLUP);

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
    sum += (digitalRead(pin) == LOW) ? 1 : 0;
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

// --- Helper: set LED color ---
void setLED(CRGB *led, CRGB color) {
  led[0] = color;
}

// Set all five LEDs to the same color
void setAll(CRGB color) {
  setLED(led1, color);
  setLED(led2, color);
  setLED(led3, color);
  setLED(led4, color);
  setLED(led5, color);
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

  if (s[0]==1 && s[1]==1 && s[2]==1) { // 1 -> all OFF
    // allOff already set
  }
  else if (s[0]==1 && s[1]==0 && s[2]==0) { // 2 -> all RED
    setAll(CRGB::Red);
  }
  else if (s[0]==0 && s[1]==1 && s[2]==0) { // 3 -> 1 Green, rest Red
    setLED(led1, CRGB::Green);
    setLED(led2, CRGB::Red);
    setLED(led3, CRGB::Red);
    setLED(led4, CRGB::Red);
    setLED(led5, CRGB::Red);
  }
  else if (s[0]==1 && s[1]==1 && s[2]==0) { // 4 -> 1 & 2 Green, rest Red
    setLED(led1, CRGB::Green);
    setLED(led2, CRGB::Green);
    setLED(led3, CRGB::Red);
    setLED(led4, CRGB::Red);
    setLED(led5, CRGB::Red);
  }
  else if (s[0]==0 && s[1]==0 && s[2]==1) { // 5 -> 1..3 Green, 4..5 Red
    setLED(led1, CRGB::Green);
    setLED(led2, CRGB::Green);
    setLED(led3, CRGB::Green);
    setLED(led4, CRGB::Red);
    setLED(led5, CRGB::Red);
  }
  else if (s[0]==1 && s[1]==0 && s[2]==1) { // 6 -> 1..4 Green, 5 Red
    setLED(led1, CRGB::Green);
    setLED(led2, CRGB::Green);
    setLED(led3, CRGB::Green);
    setLED(led4, CRGB::Green);
    setLED(led5, CRGB::Red);
  }
  else if (s[0]==0 && s[1]==1 && s[2]==1) { // 7 -> all Green
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
