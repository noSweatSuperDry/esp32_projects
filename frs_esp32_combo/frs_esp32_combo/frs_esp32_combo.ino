// Define the pin connected to the FRS sensor
#define FRS_PIN 3 // GPIO2 for ADC

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);

  // Set the ADC pin mode
  pinMode(FRS_PIN, INPUT);
}

void loop() {
  // Read the analog value from the sensor
  int analogValue = analogRead(FRS_PIN);

  // Convert the analog value to voltage (Optional)
  // ESP32 ADC has a resolution of 12 bits (0-4095)
  float voltage = analogValue * (3.3 / 4095.0); // 3.3V reference voltage

  // Print the analog and voltage values
  Serial.print("Analog Value: ");
  Serial.print(analogValue);
  Serial.print(" | Voltage: ");
  Serial.println(voltage);

  delay(500); // Wait for 500 ms before reading again
}
