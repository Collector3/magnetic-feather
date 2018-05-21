// Sample code for an ESP32 board combined with this camera from Adafruit https://www.adafruit.com/product/3202
//
// This code was cut together (and down) from a combination of:
// RepeatTimer: https://raw.githubusercontent.com/espressif/arduino-esp32/master/libraries/ESP32/examples/Timer/RepeatTimer/RepeatTimer.ino
//  License: Public domain
// CaptivePortal: https://raw.githubusercontent.com/espressif/arduino-esp32/master/libraries/DNSServer/examples/CaptivePortal/CaptivePortal.ino
//  License: LGPL Arduino Port (documentation)
// SimpleWebServer: https://raw.githubusercontent.com/espressif/arduino-esp32/master/libraries/WiFi/examples/SimpleWiFiServer/SimpleWiFiServer.ino
//  License: LGPL Arduino Port (documentation)
// Any other unique bits should also be considered as: LGPL licensed
//
#include <WiFi.h>
#include <DNSServer.h>

#define TRIG_PIN A0
#define BATTERY_V_PIN A13

DNSServer dnsServer;
WiFiServer server(80);

const byte DNS_PORT = 53;
const int timelapseIntervalSeconds = 5;

IPAddress apIpAddr(192, 168, 4, 1);
bool timelapse = false;

const char* ssid     = "CAMERA-CONTROLLER";
const char* password = "";

hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer(){
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  takePhoto();
}

void setup()
{
    Serial.begin(115200);
    
    pinMode(TRIG_PIN, OUTPUT);
    digitalWrite(TRIG_PIN, HIGH);
        
    pinMode(BATTERY_V_PIN, INPUT);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIpAddr, apIpAddr, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssid, password);
    
    Serial.println(ssid);
    Serial.println(password);
        
    dnsServer.start(DNS_PORT, "*", apIpAddr);
    server.begin();
    
    timerSemaphore = xSemaphoreCreateBinary();
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);

    timerAlarmWrite(timer, timelapseIntervalSeconds * 1000000, true);
    Serial.println("Entering main loop()");
}

// takePhoto() snaps a picture
// Line is assumed to be HIGH before calling this
void takePhoto() {        
    digitalWrite(TRIG_PIN, LOW);
    delay(50);
    Serial.println("Photo taken.");
}

// takeVideo() toggles video recording
// Line is assumed to be HIGH before calling this
void takeVideo() {
    digitalWrite(TRIG_PIN, LOW);
    delay(800);
    Serial.println("Toggled video recording."); 
}

// getBatteryVoltage() returns the internal battery voltage
// measured by a /2 resistive divider on pin A13 (specific
// to the ESP32 HUZZAH)
float getBatteryVoltage() {
    return (analogRead(BATTERY_V_PIN) * 2);
}

void loop() {
  dnsServer.processNextRequest();
  WiFiClient client = server.available();

  digitalWrite(TRIG_PIN, HIGH);

  if (client) {                             
    Serial.println("Incoming connection.");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            client.print("My IP is: ");
            client.print("<a href=\"");
            client.print("\"");
            client.print(apIpAddr);
            client.print("/");
            client.print(">");
            client.print(apIpAddr);
            client.print("</a><br><br><br>");
            
            client.print("Battery voltage is: ");
            client.print(getBatteryVoltage());
            client.print(" mV");
            client.print("<br><br><br>");
            client.print("<a href=\"/PHOTO\">TAKE A PHOTO</a><br><br><br>");
            client.print("<a href=\"/VIDEO\">START/STOP VIDEO</a><br><br><br>");
            
            if(timelapse) {
              client.print("Photo timelapse: Started<br><br>");
            } else {
              client.print("Photo timelapse: Stopped<br><br>");
            }
            
            client.print("<a href=\"/TIMELAPSE\">START/STOP PHOTO TIMELAPSE</a><br><br><br>");
                               
            client.println();
            break;
            
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }

        if (currentLine.endsWith("GET /TIMELAPSE")) {
          
          if (!timelapse) {
            
            timelapse = true;
            timerAlarmEnable(timer);
            Serial.print("Timelapse mode enabled");
            
          } else {
            
            timelapse = false;
            
            if (timer) {
              timerEnd(timer);
            }
            
            Serial.print("Timelapse mode disabled");
            
          }
        }
        
        if (currentLine.endsWith("GET /PHOTO")) {
          takePhoto();
        }
        
        if (currentLine.endsWith("GET /VIDEO")) {
          takeVideo();
        }
        
      }
    }
    
    client.stop();
    Serial.println("Connection finished.");
    
  }
}
