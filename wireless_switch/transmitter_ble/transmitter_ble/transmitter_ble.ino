#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SWITCH_PIN 2
#define RGB_BUILTIN 8  // GPIO for built-in RGB LED

// UUIDs for the service and characteristic
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic;
bool lastSwitchState = HIGH;  // Assume initial button is unpressed

void setup() {
  Serial.begin(115200);
  
  // Initialize switch
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Initialize RGB LED to red
  neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // Red color
  
  // Start BLE
  BLEDevice::init("ESP32_BLE_Transmitter");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE characteristic for the RGB color state
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  // Start the service
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setMinPreferred(0x06);  // Minimum connection interval (7.5ms)
  pAdvertising->setMaxPreferred(0x12);  // Maximum connection interval (15ms)
  pAdvertising->start();
}

void loop() {
  // Read the current switch state
  bool currentSwitchState = digitalRead(SWITCH_PIN);

  // Detect state change
  if (currentSwitchState != lastSwitchState) {
    if (currentSwitchState == LOW) {  // Button pressed
      neopixelWrite(RGB_BUILTIN, 0, 255, 0);  // Green color
      pCharacteristic->setValue("GREEN");
    } else {  // Button released
      neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // Red color
      pCharacteristic->setValue("RED");
    }
    pCharacteristic->notify();  // Notify the connected client
    delay(50);  // Debounce delay
  }

  lastSwitchState = currentSwitchState;
}
