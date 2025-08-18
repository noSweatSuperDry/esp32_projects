#include <WiFi.h>
#include <DNSServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WebServer.h>

// ==== AP credentials ====
const char* apSSID = "LT600_HUB";
const char* apPassword = "123456";

// ==== DNS ====
const byte DNS_PORT = 53;
DNSServer dnsServer;

// ==== LT600 UUIDs ====
static BLEUUID lt600ServiceUUID("0bd51666-e7cb-469b-8e4d-2742f1ba77cc");
static BLEUUID lt600NotifyUUID("e7add780-b042-4876-aae1-11285535f821");
static BLEUUID writeCharUUID("e7add780-b042-4876-aae1-11285535f721");

// ==== Web server ====
WebServer server(80);

// ==== BLE variables ====
BLEScan* pBLEScan;
BLEClient* pClient;
BLERemoteCharacteristic* pNotifyChar;
String lt600Address = "";
bool lt600Connected = false;
bool reconnectPending = false;

// ==== BLE Client Callback ====
class LT600ClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pClient) override {
        Serial.println("LT600 connected.");
    }
    void onDisconnect(BLEClient* pClient) override {
        Serial.println("LT600 disconnected. Attempting reconnect...");
        lt600Connected = false;
        reconnectPending = true;
    }
};
LT600ClientCallbacks clientCB;

// ==== Notify callback ====
static void notifyCallback(
    BLERemoteCharacteristic* pRemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify
) {
    String incoming = "";
    for (size_t i = 0; i < length; i++) incoming += (char)pData[i];
    Serial.println(incoming);
}

// ==== Connect to LT600 ====
bool connectToLT600(String address) {
    Serial.println("Connecting to LT600 at: " + address);

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(&clientCB);

    BLEAddress bleAddr(address.c_str());

    if (!pClient->connect(bleAddr)) {
        Serial.println("Failed to connect to LT600");
        return false;
    }

    BLERemoteService* pRemoteService = pClient->getService(lt600ServiceUUID);
    if (!pRemoteService) {
        Serial.println("LT600 Service not found!");
        pClient->disconnect();
        return false;
    }

    pNotifyChar = pRemoteService->getCharacteristic(lt600NotifyUUID);
    if (pNotifyChar && pNotifyChar->canNotify()) {
        pNotifyChar->registerForNotify(notifyCallback);
        lt600Connected = true;
        Serial.println("Connected and subscribed to LT600 notifications.");
        return true;
    }

    Serial.println("LT600 Notify characteristic not found!");
    pClient->disconnect();
    return false;
}

// ==== Disconnect ====
void disconnectLT600() {
    if (pClient && pClient->isConnected()) {
        pClient->disconnect();
    }
    lt600Connected = false;
    reconnectPending = false;
}

// ==== Scan for LT600 ====
String scanForLT600() {
    Serial.println("Scanning for LT600...");
    String resultHTML = "<h2>Scan Results</h2><ul>";

    BLEScanResults* results = pBLEScan->start(3);
    int count = results->getCount();

    for (int i = 0; i < count; i++) {
        BLEAdvertisedDevice dev = results->getDevice(i);
        if (dev.haveServiceUUID() && dev.isAdvertisingService(lt600ServiceUUID)) {
            String addr = dev.getAddress().toString().c_str();
            lt600Address = addr;
            resultHTML += "<li>LT600 found at " + addr +
                          " <form style='display:inline;' action='/connect' method='POST'><button type='submit'>Connect</button></form></li>";
        }
    }

    if (lt600Address == "") {
        resultHTML += "<li>No LT600 found.</li>";
    }

    resultHTML += "</ul>";
    pBLEScan->clearResults();
    return resultHTML;
}

// ==== Root Page ====
void handleRoot() {
    String page = "<html><head><meta charset='UTF-8'></head><body>";
    page += "<h1>LT600 BLE Control</h1>";

    if (lt600Connected) {
        page += "<p>Status: <b style='color:green;'>Connected</b></p>";
        page += "<form action='/disconnect' method='POST'><button type='submit'>Disconnect</button></form>";
    } else {
        page += "<p>Status: <b style='color:red;'>Disconnected</b></p>";
        page += "<form action='/scan' method='GET'><button type='submit'>Scan for LT600</button></form>";
    }

    page += "</body></html>";
    server.send(200, "text/html", page);
}

// ==== Scan Page ====
void handleScan() {
    String page = "<html><head><meta charset='UTF-8'></head><body>";
    page += "<h1>LT600 Scan</h1>";
    page += scanForLT600();
    page += "<br><a href='/'>Back</a>";
    page += "</body></html>";
    server.send(200, "text/html", page);
}

// ==== Connect Handler ====
void handleConnect() {
    if (lt600Address != "" && !lt600Connected) {
        connectToLT600(lt600Address);
    }
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

// ==== Disconnect Handler ====
void handleDisconnect() {
    disconnectLT600();
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

// ==== Setup ====
void setup() {
    Serial.begin(115200);

    // --- WiFi AP Mode ---
    WiFi.softAP(apSSID, apPassword);
    IPAddress myIP = WiFi.softAPIP();
    Serial.printf("WiFi AP started: %s\n", apSSID);
    Serial.printf("AP IP address: %s\n", myIP.toString().c_str());

    // --- DNS server for captive portal ---
    dnsServer.start(DNS_PORT, "*", myIP);

    // --- BLE Init ---
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);

    // --- WebServer Handlers ---
    server.on("/", handleRoot);
    server.on("/scan", handleScan);
    server.on("/connect", HTTP_POST, handleConnect);
    server.on("/disconnect", HTTP_POST, handleDisconnect);

    server.begin();
    Serial.println("HTTP server started");
}

// ==== Loop ====
void loop() {
    dnsServer.processNextRequest();  // Captive portal DNS
    server.handleClient();

    // Auto-reconnect BLE if needed
    if (!lt600Connected && reconnectPending && lt600Address != "") {
        Serial.println("Reconnecting to LT600...");
        reconnectPending = false;
        connectToLT600(lt600Address);
    }
}
