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
void startupBlink() {
  for (int i = 0; i < 2; i++) {
    setLED(led1, CRGB::Green);
    setLED(led2, CRGB::Green);
    setLED(led3, CRGB::Green);
    setLED(led4, CRGB::Green);
    setLED(led5, CRGB::Green);
    FastLED.show();
    delay(250);

    setLED(led1, CRGB::Red);
    setLED(led2, CRGB::Red);
    setLED(led3, CRGB::Red);
    setLED(led4, CRGB::Red);
    setLED(led5, CRGB::Red);
    FastLED.show();
    delay(250);
  }
}
void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812, LED_PIN_1, GRB>(led1, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_2, GRB>(led2, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_3, GRB>(led3, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_4, GRB>(led4, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_5, GRB>(led5, NUM_LEDS);

  pinMode(IN_PIN_1, INPUT);
  pinMode(IN_PIN_2, INPUT);
  pinMode(IN_PIN_3, INPUT);

  // Show blink sequence at startup
  startupBlink();

  // LED setup
  FastLED.addLeds<WS2812, LED_PIN_1, GRB>(led1, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_2, GRB>(led2, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_3, GRB>(led3, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_4, GRB>(led4, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_5, GRB>(led5, NUM_LEDS);

  // Input setup
  pinMode(IN_PIN_1, INPUT);
  pinMode(IN_PIN_2, INPUT);
  pinMode(IN_PIN_3, INPUT);

  allOff();
  FastLED.show();
}

void loop() {
  int s[3];
  s[0] = digitalRead(IN_PIN_1);
  s[1] = digitalRead(IN_PIN_2);
  s[2] = digitalRead(IN_PIN_3);

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

// --- Helper: turn off all LEDs ---
void allOff() {
  setLED(led1, CRGB::Black);
  setLED(led2, CRGB::Black);
  setLED(led3, CRGB::Black);
  setLED(led4, CRGB::Black);
  setLED(led5, CRGB::Black);
  FastLED.show();
}

// --- Main LED logic ---
void applyLEDLogic(int s[]) {
  if (s[0]==1 && s[1]==1 && s[2]==1) { // 1
    allOff();
  } 
  else if (s[0]==1 && s[1]==0 && s[2]==0) { // 2
    setLED(led1, CRGB::Red);
    setLED(led2, CRGB::Red);
    setLED(led3, CRGB::Red);
    setLED(led4, CRGB::Red);
    setLED(led5, CRGB::Red);
  }
  else if (s[0]==0 && s[1]==1 && s[2]==0) { // 3
    setLED(led1, CRGB::Green);
    setLED(led2, CRGB::Red);
    setLED(led3, CRGB::Red);
    setLED(led4, CRGB::Red);
    setLED(led5, CRGB::Red);
  }
  else if (s[0]==1 && s[1]==1 && s[2]==0) { // 4
    setLED(led1, CRGB::Green);
    setLED(led2, CRGB::Green);
    setLED(led3, CRGB::Red);
    setLED(led4, CRGB::Red);
    setLED(led5, CRGB::Red);
  }
  else if (s[0]==0 && s[1]==0 && s[2]==1) { // 5
    setLED(led1, CRGB::Green);
    setLED(led2, CRGB::Green);
    setLED(led3, CRGB::Green);
    setLED(led4, CRGB::Red);
    setLED(led5, CRGB::Red);
  }
  else if (s[0]==1 && s[1]==0 && s[2]==1) { // 6
    setLED(led1, CRGB::Green);
    setLED(led2, CRGB::Green);
    setLED(led3, CRGB::Green);
    setLED(led4, CRGB::Green);
    setLED(led5, CRGB::Red);
  }
  else if (s[0]==0 && s[1]==1 && s[2]==1) { // 7
    setLED(led1, CRGB::Green);
    setLED(led2, CRGB::Green);
    setLED(led3, CRGB::Green);
    setLED(led4, CRGB::Green);
    setLED(led5, CRGB::Green);
  }
 else if (s[0]==0 && s[1]==0 && s[2]==0) { // 8 - Blink Green/Red 2 times
  for (int i = 0; i < 2; i++) {
    setLED(led1, CRGB::Green);
    setLED(led2, CRGB::Green);
    setLED(led3, CRGB::Green);
    setLED(led4, CRGB::Green);
    setLED(led5, CRGB::Green);
    FastLED.show();
    delay(250);

    setLED(led1, CRGB::Red);
    setLED(led2, CRGB::Red);
    setLED(led3, CRGB::Red);
    setLED(led4, CRGB::Red);
    setLED(led5, CRGB::Red);
    FastLED.show();
    delay(250);
  }
}
  FastLED.show();
}
