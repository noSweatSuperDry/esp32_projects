#include <ArduinoBLE.h>
#include <ESPAsyncWebServer.h>

// Replace with your Wi-Fi credentials
const char* ssid = "Your_SSID";
const char* password = "Your_Password";

// Define the IP address for the HTTP server
IPAddress local_IP(192, 168, 100, 100);

// Create an AsyncWebServer instance
AsyncWebServer server(80);

// Global variable to store the selected device address
BLEDevice* selectedDevice = nullptr;

// Function to scan for BLE devices and update the web page
void handleScan() {
  BLEDevice::scan(5000, 0);
  BLEDevice* device = BLEDevice::first();

  String deviceList = "<ul>";
  while (device) {
    deviceList += "<li>" + device->getAddress().toString() + "</li>";
    device = device->next();
  }
  deviceList += "</ul>";

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<h1>ESP32 BLE Device Selection</h1>" + deviceList);
  });
}

void setup() {
  Serial.begin(115200);

  // Initialize Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Initialize BLE
  BLEDevice::init();
  BLEDevice::eraseBondingData();

  // Configure HTTP server
  server.begin();
  Serial.println("HTTP server started");

  // Initial scan to populate the device list
  handleScan();
}

void loop() {
  // Handle HTTP requests
  server.handleClient();

  // Periodically scan for devices
  if (millis() - lastScanTime > 10000) {
    handleScan();
    lastScanTime = millis();
  }

  // If a device is selected, try to connect
  if (selectedDevice && !selectedDevice->isConnected()) {
    selectedDevice->connect();
    // Handle connection success or failure
    if (selectedDevice->isConnected()) {
      Serial.println("Connected to device");
      // ... (further connection logic)
    } else {
      Serial.println("Failed to connect");
      selectedDevice = nullptr;
    }
  }
}