#include <WiFi.h>

const char* ssid = "EASY";
const char* password = "easymesh";

WiFiServer server(80);

String header;
// init state of leds
String yellowLEDState = "off";
String rgbLEDState = "off";

const int yellowLED = 5;
const int RGB_LED_PIN = RGB_BUILTIN;  // Built-in RGB LED pin

unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;

// color value 
int red = 0, green = 0, blue = 0;

void setup() {
  Serial.begin(115200);

  // Initialize the output variables as outputs
  pinMode(yellowLED, OUTPUT);
  digitalWrite(yellowLED, LOW);

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          if (currentLine.length() == 0) {
            // HTTP headers and response code
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /5/on") >= 0) {
              Serial.println("GPIO 5 on");
              yellowLEDState = "on";
              digitalWrite(yellowLED, HIGH);
             
            } else if (header.indexOf("GET /5/off") >= 0) {
              Serial.println("GPIO 5 off");
              yellowLEDState = "off";
              digitalWrite(yellowLED, LOW);
            }

            // Control RGB LED color
            if (header.indexOf("GET /rgb/red") >= 0) {
              Serial.println("RGB Red");
              rgbLEDState = "Red";
              red = 255; green = 0; blue = 0;
            } else if (header.indexOf("GET /rgb/green") >= 0) {
              Serial.println("RGB Green");
              rgbLEDState = "Green";
              red = 0; green = 255; blue = 0;
            } else if (header.indexOf("GET /rgb/blue") >= 0) {
              Serial.println("RGB Blue");
              rgbLEDState = "Blue";
              red = 0; green = 0; blue = 255;
            } else if (header.indexOf("GET /rgb/off") >= 0) {
              Serial.println("RGB Off");
              rgbLEDState = "Off";
              red = 0; green = 0; blue = 0;
            }
            neopixelWrite(RGB_LED_PIN, red, green, blue);  // Write to RGB LED

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");
            
            // Display current state, and ON/OFF buttons for GPIO 5
            client.println("<p>GPIO 5 - Yellow LED - State " + yellowLEDState + "</p>");
            if (yellowLEDState == "off") {
              client.println("<p><a href=\"/5/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/5/off\"><button class=\"button button2\">OFF</button></a></p>");
            }

            // Display RGB LED color buttons
            client.println("<p>RGB LED - State " + rgbLEDState + "</p>");
            client.println("<p><a href=\"/rgb/red\"><button class=\"button\">Red</button></a>");
            client.println("<a href=\"/rgb/green\"><button class=\"button\">Green</button></a>");
            client.println("<a href=\"/rgb/blue\"><button class=\"button\">Blue</button></a>");
            client.println("<a href=\"/rgb/off\"><button class=\"button button2\">OFF</button></a></p>");
            client.println("</body></html>");
            
            // End of HTTP response
            client.println();
            break;
          } else { 
            currentLine = "";
          }
        } else if (c != '\r') {  
          currentLine += c;
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
