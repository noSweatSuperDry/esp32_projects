const int padPin = 3;             // Pressure pad GPIO
const int gpioPinSwitch = 9;      // Output GPIO

unsigned long lastTriggerTime = 0;
bool hasTriggered = false;

void setup() {
  Serial.begin(115200);
  pinMode(padPin, INPUT_PULLUP);       // Internal pull-up enabled
  pinMode(gpioPinSwitch, OUTPUT);
  digitalWrite(gpioPinSwitch, LOW);    // Ensure output is LOW initially
}

void loop() {
  unsigned long currentTime = millis();
  bool padPressed = digitalRead(padPin) == LOW;

  // Check if cooldown has passed
  if ((currentTime - lastTriggerTime) >= 50000) {
    hasTriggered = false;
  }

  if (padPressed && !hasTriggered) {
    Serial.println("Pad Pressed - GPIO9 ON");
    digitalWrite(gpioPinSwitch, HIGH);
    delay(1000);  // Keep GPIO9 ON for 1 second
    digitalWrite(gpioPinSwitch, LOW);
    Serial.println("GPIO9 OFF - Entering Cooldown");
    
    lastTriggerTime = millis();  // Mark cooldown start
    hasTriggered = true;
  }

  // Optional small delay to avoid busy looping
  delay(10);
}

