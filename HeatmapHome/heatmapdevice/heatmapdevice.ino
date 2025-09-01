#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "SKYNET MESH NETWORK";
const char* password = "Evan.Sejnin.15";

WebServer server(80);

void handleRSSI() {
  long rssi = WiFi.RSSI();
  server.send(200, "text/plain", String(rssi));
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  server.on("/rssi", handleRSSI);
  server.begin();
}

void loop() {
  server.handleClient();
}
