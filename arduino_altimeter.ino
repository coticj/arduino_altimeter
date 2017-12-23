#define PIN 4   

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <SPIFFS.h>

#define PIN 4   
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>




#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel strip = Adafruit_NeoPixel(4, PIN, NEO_GRB + NEO_KHZ800);           
int aboveCheck = 0;
int aboveBreak = 0;
double baseline, alti;

//settings
int checkAlti = 300; // we passed 300, signal
int breakAlti = 1550; // breakoff, signal
int openAlti = 1000; // we are open, stop signaling

extern const uint8_t gamma8[];

//colors
uint32_t red = strip.Color(255, 0, 0);
uint32_t green = strip.Color(0, 255, 0);
uint32_t blue = strip.Color(0, 0, 255);
uint32_t white = strip.Color(127, 127, 127);
uint32_t orange = strip.Color(pgm_read_byte(&gamma8[255]), pgm_read_byte(&gamma8[165]), pgm_read_byte(&gamma8[0]));


WebServer server(80);
Adafruit_BMP280 bmp; // I2C

//initialize color correction
extern const uint8_t gamma8[];

// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 1000;           // interval at which to blink (milliseconds)

#define DBG_OUTPUT_PORT Serial

const char* ssid = "alti-jure";
const char* password = "altialti";
const char* host = "alti1";

void setup()
{
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.print("\n");
  DBG_OUTPUT_PORT.setDebugOutput(true);
 
  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  

  
  bmp.begin();
  httpServer();
  
  DBG_OUTPUT_PORT.println("HTTP server started");

  baseline=getBaseline();
  Serial.println(baseline);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  flashStrip(green, 2,500); // Green, signal ready: 3 times 100ms delay
  
}

void loop() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    alti=bmp.readAltitude(baseline*100);
    Serial.print(F("Curr alt = "));
    Serial.print(alti); //
    Serial.println(" m");
    Serial.println();
    
    if (alti > checkAlti && aboveCheck == 0) {
      aboveCheck = 1;
      flashStrip(blue,3,200); //we passed 300m, flash
    }

    if (alti > breakAlti && aboveBreak == 0) {
      aboveBreak = 1;
      flashStrip(red,2,500); //we passed breakoff altitude, flash
    }

    if (alti < breakAlti && aboveBreak == 1) {
     flashStrip(red,20,200);
    }

    if (alti < openAlti && aboveBreak == 1) {
      aboveBreak = 0;
    }
    
  }
      
   server.handleClient();
}

float getBatteryVoltage()
{
  float measuredvbat = analogRead(A13);
  Serial.print(measuredvbat);
  Serial.print(" = ");

  measuredvbat *= 2;    // we divided by 2, so multiply back
  //measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage

  return measuredvbat;
}

float getBatteryPercentage()
{
  return _min(map(getBatteryVoltage() * 10, 3.35 * 10, 4.5 * 10, 0, 100), 100); // Calculate Battery Level (Percent)
}

float getTemperature()
{
 return bmp.readTemperature();
}

float getPressure()
{
 return bmp.readPressure();
}

float getAltitude(float baseline)
{
  return bmp.readAltitude(baseline);
}
