#include <WiFi.h>
#include <WiFiServer.h>

#define WIFI_SSID "EcoaimsSwitch_01"
#define WIFI_PASSWORD "123456789"
#define RGB_BUILTIN 8  // GPIO for built-in RGB LED is now GPIO8

WiFiServer server(8080);  // Listen on port 8080

void setup() {
  Serial.begin(115200);
  
  // Set initial RGB color to red using neopixelWrite
  neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // Red color

  // Connect to Wi-Fi
  WiFi.config(IPAddress(192, 168, 1, 20), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0)); // Static IP configuration
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Start the server
  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client connected");

    while (client.connected()) {
      if (client.available()) {
        String message = client.readStringUntil('\n');
        message.trim();

        if (message == "COLOR_GREEN") {
          // Change the RGB LED color to green
          neopixelWrite(RGB_BUILTIN, 0, 255, 0);  // Green color
        } else if (message == "COLOR_RED") {
          // Change the RGB LED color to red
          neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // Red color
        }
      }
    }
    client.stop();
    Serial.println("Client disconnected");
  }
}
