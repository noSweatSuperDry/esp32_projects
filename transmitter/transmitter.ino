// Define the GPIO pin for the switch
#define SWITCH_PIN 22

// Define the initial LED state (0 for red, 1 for green)
int ledState = 0;  // 0 means red, 1 means green

void setup() {
  // Initialize the switch pin as input
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Set the initial LED state to red (RGB = 255, 0, 0)
  neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // Red
}

void loop() {
  // Check if the switch is pressed (assuming active-low button)
  if (digitalRead(SWITCH_PIN) == LOW) {
    delay(200);  // Debounce delay to prevent multiple triggers from one press

    // Toggle the LED state
    if (ledState == 0) {
      // Turn the LED green
      neopixelWrite(RGB_BUILTIN, 0, RGB_BRIGHTNESS, 0);  // Green
      ledState = 1;  // Update the LED state
    } else {
      // Turn the LED red
      neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // Red
      ledState = 0;  // Update the LED state
    }
  }
}
