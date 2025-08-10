#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WebServer.h>

// ==== Wi-Fi credentials ====
const char* ssid = "SKYNET MESH NETWORK";
const char* password = "Evan.Sejnin.15";

// ==== LT600 UUIDs ====
static BLEUUID serviceUUID("0bd51666-e7cb-469b-8e4d-2742f1ba77cc");
static BLEUUID writeCharUUID("e7add780-b042-4876-aae1-11285535f721");
static BLEUUID notifyCharUUID("e7add780-b042-4876-aae1-11285535f821");

// ==== Web server ====
WebServer server(80);
String scanResultsHTML;
String liveDataHTML;

// ==== BLE ====
BLEScan* pBLEScan;
BLEClient* pClient;
BLERemoteCharacteristic* pNotifyChar;
String connectedAddress = "";

// Notification callback
static void notifyCallback(
    BLERemoteCharacteristic* pRemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify
) {
    String incoming = "";
    for (size_t i = 0; i < length; i++) incoming += (char)pData[i];
    Serial.println("BLE Data: " + incoming);

    // Append to live data buffer for web display
    liveDataHTML += incoming + "<br>";
    if (liveDataHTML.length() > 5000) liveDataHTML = ""; // prevent overflow
}

// Connect to device by address
bool connectToDevice(String address) {
    Serial.println("Connecting to: " + address);

    pClient = BLEDevice::createClient();
    BLEAddress bleAddr(address.c_str());

    if (!pClient->connect(bleAddr)) {
        Serial.println("Failed to connect!");
        return false;
    }

    Serial.println("Connected, discovering service...");
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.println("Service not found!");
        pClient->disconnect();
        return false;
    }

    pNotifyChar = pRemoteService->getCharacteristic(notifyCharUUID);
    if (pNotifyChar && pNotifyChar->canNotify()) {
        pNotifyChar->registerForNotify(notifyCallback);
        Serial.println("Notify characteristic subscribed!");
        return true;
    }

    Serial.println("Notify characteristic not found!");
    pClient->disconnect();
    return false;
}

// Scan for BLE devices
void scanForDevices() {
    scanResultsHTML = "<h2>BLE Scan Results</h2><ul>";

    Serial.println("Scanning for BLE devices...");
    BLEScanResults* results = pBLEScan->start(3);
    int count = results->getCount();
    Serial.printf("Found %d devices\n", count);

    for (int i = 0; i < count; i++) {
        BLEAdvertisedDevice dev = results->getDevice(i);
        String addr = dev.getAddress().toString().c_str();
        scanResultsHTML += "<li>" + String(dev.toString().c_str());
        scanResultsHTML += " <a href='/connect?addr=" + addr + "'>Connect</a></li>";
    }

    scanResultsHTML += "</ul>";
    pBLEScan->clearResults();
}

// Handle main page
void handleRoot() {
    scanForDevices();
    String page = "<html><head><meta charset='UTF-8'></head><body>";
    page += "<h1>ESP32-C6 BLE Scanner</h1>";
    page += scanResultsHTML;
    page += "<br><form action='/' method='GET'><input type='submit' value='Scan Again'></form>";
    page += "<h2>Live Data</h2>";
    page += liveDataHTML;
    page += "</body></html>";
    server.send(200, "text/html", page);
}

// Handle connect request
void handleConnect() {
    if (server.hasArg("addr")) {
        String addr = server.arg("addr");
        if (connectToDevice(addr)) {
            connectedAddress = addr;
            liveDataHTML += "<b>Connected to " + addr + "</b><br>";
        } else {
            liveDataHTML += "<b>Failed to connect to " + addr + "</b><br>";
        }
    }
    handleRoot();
}

void setup() {
    Serial.begin(115200);

    // Wi-Fi connect
    WiFi.begin(ssid, password);
    Serial.printf("Connecting to %s", ssid);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());

    // BLE init
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);

    // Web server routes
    server.on("/", handleRoot);
    server.on("/connect", handleConnect);
    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
}
