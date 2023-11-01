/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

// Import required libraries
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "AsyncTCP.h"
#include <Adafruit_NeoPixel.h>
#include <mDNS.h>

// Replace with your network credentials
const char* ssid = "Hotspot";
const char* password = "Password";
#define HOSTNAME "lampi"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Set LED GPIO
const int ledPin = D6;

// Stores LED state
String ledState;

// Set Neopixel
#define NEO_PIN D1
#define LED_COUNT 12
String neoState = "";

unsigned long previousMillis = 0; 
const long interval = 50;  // Change rate for the transitions
int targetR, targetG, targetB;
int currentR = 0, currentG = 0, currentB = 0;
float stepR, stepG, stepB;
int steps = 0;
bool relaxMode = false;

// Create Neopixel object
Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRBW + NEO_KHZ800);

// Replaces placeholder with LED state value
String processor(const String& var){
  Serial.println(var);
  if(var == "LEDSTATE"){
    if(digitalRead(ledPin)){
      ledState = "ON";
    }
    else{
      ledState = "OFF";
    }
    Serial.print(ledState);
    return ledState;
  } 
   
  if (var == "NEOSTATE") {
    return neoState;
  } else if (var == "NEOCOLOR") {
    return neoState;  // Assuming neoState holds the current hex color value
  }

  return String();
}

// Helper function for Neopixel
void hexStringToRGB(String hex, uint8_t &r, uint8_t &g, uint8_t &b) {
    // Remove leading #
    hex.remove(0, 1);

    // Convert hex string to integer values
    r = strtol(hex.substring(0, 2).c_str(), NULL, 16);
    g = strtol(hex.substring(2, 4).c_str(), NULL, 16);
    b = strtol(hex.substring(4, 6).c_str(), NULL, 16);
}
 
void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  // Init Neopixel
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  for(int i = 0; i < 12; i++){
    strip.setPixelColor(i, 0, 0, 0, 32);  
    delay(50);
    strip.show();
  }

  for(int i = 0; i < 12; i++){
    strip.setPixelColor(i, 0, 0, 0, 0);  
    delay(50);
    strip.show();
  }

  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  // this advertises the device locally at "coolname.local"
  mdns_init(); 
  mdns_hostname_set(HOSTNAME); 
  mdns_instance_name_set(HOSTNAME); 
  Serial.printf("MDNS responder started at http://%s.local\n", HOSTNAME);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  // Route to load bootstrap.bundle.js file
  server.on("/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/bootstrap.bundle.min.js", "application/javascript");
  });

  // Route to load bootstrap.bundle.js.map file
  server.on("/bootstrap.bundle.min.js.map", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/bootstrap.bundle.min.js.map", "application/json");
  });

  // Route to load bootstrap.min.css file
  server.on("/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/bootstrap.min.css", "text/css");
  });

  // Route to set GPIO to HIGH
  server.on("/ledon", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(ledPin, HIGH);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // Route to set GPIO to LOW
  server.on("/ledoff", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(ledPin, LOW);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/setneocolor", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("color")) {
          String hexColor = request->getParam("color")->value();
          uint8_t red, green, blue;
          hexStringToRGB(hexColor, red, green, blue);

          for(int i = 0; i < LED_COUNT; i++) {
              strip.setPixelColor(i, red, green, blue, 0);
              strip.show();
          }

          neoState = hexColor;
      }
      request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/neooff", HTTP_GET, [](AsyncWebServerRequest *request){  
    for(int i = 0; i < LED_COUNT; i++){
      strip.setPixelColor(i, 0, 0, 0, 0);  
      //delay(100);
      relaxMode = false; // Disable relax mode
      strip.show();
    }
    neoState = "Off";
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/neorelax", HTTP_GET, [](AsyncWebServerRequest *request){
      relaxMode = !relaxMode;  // Toggle the relax mode on and off
      if(relaxMode) {
          neoState = "Relax Mode";
      } else {
          for(int i = 0; i < LED_COUNT; i++){
              strip.setPixelColor(i, 0, 0, 0, 0);
              strip.show();
          }
          neoState = "Off";
      }
      request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Start server
  server.begin();
}

void loop(){
  if(relaxMode) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        
        if(steps <= 0) {
            targetR = random(0, 256);
            targetG = random(0, 256);
            targetB = random(0, 256);
            
            steps = 100;  // Number of steps for transition
            stepR = (targetR - currentR) / (float)steps;
            stepG = (targetG - currentG) / (float)steps;
            stepB = (targetB - currentB) / (float)steps;
        } else {
            currentR += stepR;
            currentG += stepG;
            currentB += stepB;
            
            for(int i = 0; i < LED_COUNT; i++) {
                strip.setPixelColor(i, currentR, currentG, currentB, 0);
                strip.show();
            }

            steps--;
        }
    }
  }
}