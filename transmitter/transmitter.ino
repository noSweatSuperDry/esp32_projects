#include <WiFi.h>
#include <WiFiClient.h>

#define WIFI_SSID "EcoaimsSwitch_01"
#define WIFI_PASSWORD "123456789"
#define SWITCH_PIN 22
#define RGB_BUILTIN 8  // GPIO for built-in RGB LED is now GPIO8

WiFiClient client;

const char* receiver_ip = "192.168.1.20";  // IP of the receiver ESP32
const int receiver_port = 8080;

void setup() {
  Serial.begin(115200);
  
  // Initialize Switch
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  
  // Set initial RGB color to red using neopixelWrite
  neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // Red color
  
  // Connect to Wi-Fi
  WiFi.config(IPAddress(192, 168, 1, 10), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0)); // Static IP configuration
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Connect to the receiver
  if (!client.connect(receiver_ip, receiver_port)) {
    Serial.println("Connection to receiver failed");
    return;
  }
  Serial.println("Connected to receiver");
}

void loop() {
  // Check switch state
  if (digitalRead(SWITCH_PIN) == LOW) { // Switch is pressed
    // Change the RGB LED color to green
    neopixelWrite(RGB_BUILTIN, 0, 255, 0);  // Green color

    // Send signal to receiver
    if (client.connected()) {
      client.println("COLOR_GREEN");
    }
    delay(200); // Debounce delay
  } else {
    // Change the RGB LED color to red
    neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // Red color

    // Send signal to receiver
    if (client.connected()) {
      client.println("COLOR_RED");
    }
    delay(200); // Debounce delay
  }
}
