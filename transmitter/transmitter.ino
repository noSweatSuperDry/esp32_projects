// Define the GPIO pin for the switch
#define SWITCH_PIN 22

// Define the initial LED state (0 for red, 1 for green)
int ledState = 0;  // 0 means red, 1 means green

// Variable to track button state
bool buttonPressed = false;

void setup() {
  // Initialize the switch pin as input
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Set the initial LED state to red (RGB = 255, 0, 0)
  neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // Red
}

void loop() {
  // Read the state of the button (LOW when pressed)
  bool currentButtonState = digitalRead(SWITCH_PIN) == LOW;

  // If the button is pressed and it was not pressed before
  if (currentButtonState && !buttonPressed) {
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
    
    // Mark that the button has been pressed
    buttonPressed = true;
    
    // Add a small delay for debouncing
    delay(200);
  }

  // If the button is released, reset the buttonPressed flag
  if (!currentButtonState) {
    buttonPressed = false;
  }
}
