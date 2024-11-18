#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

#define RGB_BUILTIN 8  // GPIO for built-in RGB LED
#define GPIO_PIN 22     // GPIO for additional control (ON/OFF)

// UUIDs for the server's service and characteristic
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

bool deviceConnected = false;

// Callback class to handle connection events
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pClient) {
    deviceConnected = true;
    Serial.println("Connected to BLE Server");
  }

  void onDisconnect(BLEClient* pClient) {
    deviceConnected = false;
    Serial.println("Disconnected from BLE Server");
  }
};

// Notification callback function
void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  
  // Convert notification data to a String
  String value = String((char*)pData).substring(0, length);
  value.trim(); // Remove any extra whitespace

  Serial.print("Received notification: ");
  Serial.println(value);

  // Check for exact match to "GREEN" or "RED" only
  if (value == "GREEN") {
    neopixelWrite(RGB_BUILTIN, 0, 255, 0);  // Green color
    digitalWrite(GPIO_PIN, HIGH);          // Turn GPIO 5 ON
    Serial.println("Receiver LED set to GREEN, GPIO 5 ON");
  } else if (value == "RED") {
    neopixelWrite(RGB_BUILTIN, 255, 0, 0); // Red color
    digitalWrite(GPIO_PIN, LOW);           // Turn GPIO 5 OFF
    Serial.println("Receiver LED set to RED, GPIO 5 OFF");
  } else {
    Serial.println("Received unexpected notification value.");
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize RGB LED to red initially
  neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // Red color

  // Configure GPIO pin
  pinMode(GPIO_PIN, OUTPUT);
  digitalWrite(GPIO_PIN, LOW);  // Ensure GPIO 5 starts OFF

  // Start BLE client
  BLEDevice::init("");
  BLEClient *pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the BLE Server using its MAC address
  BLEAddress serverAddress("40:4c:ca:5c:1a:6e");  // Replace with BLE server's MAC address
  Serial.print("Connecting to server at ");
  Serial.println(serverAddress.toString().c_str());

  if (!pClient->connect(serverAddress)) {
    Serial.println("Failed to connect to BLE server");
    return;
  }
  
  // Obtain reference to the remote service on the server
  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("Failed to find the service UUID");
    return;
  }

  // Obtain reference to the characteristic on the server
  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("Failed to find the characteristic UUID");
    return;
  }
  
  // Subscribe to notifications on the characteristic
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Registered for notifications");
  }
}

void loop() {
  // No additional code needed here; LED updates happen in the notification callback
}
