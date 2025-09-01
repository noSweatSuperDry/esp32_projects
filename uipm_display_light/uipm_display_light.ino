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

CRGB** gLedMap = nullptr;

// --- Input pins ---
#define IN_PIN_1 8
#define IN_PIN_2 9
#define IN_PIN_3 10

#define INPUT_ACTIVE_LOW 1
#define LED_ORDER_REVERSED 0
#define DEBUG_RAW_READS 0

int lastState[3] = { -1, -1, -1 };

// --- Blink state machine ---
bool blinking = false;
unsigned long blinkTimer = 0;
int blinkStep = 0;

// Forward declarations
void setLED(CRGB *led, CRGB color);
void setLEDAt(int idx, CRGB color);
void setAll(CRGB color);
void allOff();
void applyLEDLogic(int s[]);

void setup() {
  Serial.begin(115200);
  Serial.println("Device Started");

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
  gLedMap = (CRGB**)ledMapLocal;

#if INPUT_ACTIVE_LOW
  pinMode(IN_PIN_1, INPUT_PULLUP);
  pinMode(IN_PIN_2, INPUT_PULLUP);
  pinMode(IN_PIN_3, INPUT_PULLUP);
#else
  pinMode(IN_PIN_1, INPUT_PULLDOWN);
  pinMode(IN_PIN_2, INPUT_PULLDOWN);
  pinMode(IN_PIN_3, INPUT_PULLDOWN);
#endif

  allOff();
}

int readInputLogical(int pin) {
  const int samples = 3;
  int sum = 0;
  for (int i = 0; i < samples; i++) {
    int raw = digitalRead(pin);
#if DEBUG_RAW_READS
    Serial.print("raw("); Serial.print(pin); Serial.print(")="); Serial.print(raw); Serial.print(" ");
#endif
    if (INPUT_ACTIVE_LOW) sum += (raw == LOW) ? 1 : 0;
    else sum += (raw == HIGH) ? 1 : 0;
  }
  return (sum > samples / 2) ? 1 : 0;
}

void loop() {
  int s[3];
  s[0] = readInputLogical(IN_PIN_1);
  s[1] = readInputLogical(IN_PIN_2);
  s[2] = readInputLogical(IN_PIN_3);

  // Always check blinking state machine
  if (blinking) {
    unsigned long now = millis();
    if (now - blinkTimer > 250) {
      blinkTimer = now;
      if (blinkStep % 2 == 0) {
        setAll(CRGB::Green);
        Serial.println("Blink: Green");
      } else {
        setAll(CRGB::Red);
        Serial.println("Blink: Red");
      }
      FastLED.show();
      blinkStep++;
      if (blinkStep >= 4) {
        blinking = false;
        Serial.println("Blink sequence finished.");
      }
    }
    return; // skip normal logic while blinking
  }

  // Only update if input changed
  if (s[0] != lastState[0] || s[1] != lastState[1] || s[2] != lastState[2]) {
    Serial.print("GPIO State Changed: ");
    Serial.print(s[0]); Serial.print(",");
    Serial.print(s[1]); Serial.print(",");
    Serial.println(s[2]);

    applyLEDLogic(s);

    lastState[0] = s[0];
    lastState[1] = s[1];
    lastState[2] = s[2];
  }
}

void setLED(CRGB *led, CRGB color) {
  led[0] = color;
}

void setLEDAt(int idx, CRGB color) {
  if (gLedMap == nullptr || idx < 0 || idx > 4) return;
  setLED(gLedMap[idx], color);
}

void setAll(CRGB color) {
  for (int i = 0; i < 5; ++i) setLEDAt(i, color);
}

void allOff() {
  setAll(CRGB::Black);
  FastLED.show();
}

void applyLEDLogic(int s[]) {
  setAll(CRGB::Black);

  if (s[0]==0 && s[1]==0 && s[2]==0) { 
    Serial.println("Case 1: All OFF");
  }
  else if (s[0]==0 && s[1]==1 && s[2]==1) { 
    Serial.println("Case 2: All RED");
    setAll(CRGB::Red);
  }
  else if (s[0]==1 && s[1]==0 && s[2]==1) {
    Serial.println("Case 3: First Green, rest Red");
    setLEDAt(0, CRGB::Green);
    for (int i = 1; i < 5; i++) setLEDAt(i, CRGB::Red);
  }
  else if (s[0]==0 && s[1]==0 && s[2]==1) {
    Serial.println("Case 4: First 2 Green, rest Red");
    setLEDAt(0, CRGB::Green);
    setLEDAt(1, CRGB::Green);
    for (int i = 2; i < 5; i++) setLEDAt(i, CRGB::Red);
  }
  else if (s[0]==1 && s[1]==1 && s[2]==0) {
    Serial.println("Case 5: First 3 Green, last 2 Red");
    for (int i = 0; i < 3; i++) setLEDAt(i, CRGB::Green);
    for (int i = 3; i < 5; i++) setLEDAt(i, CRGB::Red);
  }
  else if (s[0]==0 && s[1]==1 && s[2]==0) {
    Serial.println("Case 6: First 4 Green, last Red");
    for (int i = 0; i < 4; i++) setLEDAt(i, CRGB::Green);
    setLEDAt(4, CRGB::Red);
  }
  else if (s[0]==1 && s[1]==0 && s[2]==0) {
    Serial.println("Case 7: All Green");
    setAll(CRGB::Green);
  }
  else if (s[0]==1 && s[1]==1 && s[2]==1) {
    Serial.println("Case 8: Blink Green/Red");
    blinking = true;
    blinkStep = 0;
    blinkTimer = millis();
  }

  FastLED.show();
}
