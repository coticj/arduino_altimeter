#define PIN 4
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_NeoPixel.h>

Adafruit_BMP280 bmp; // I2C
Adafruit_NeoPixel strip = Adafruit_NeoPixel(4, PIN, NEO_GRB + NEO_KHZ800);
int aboveCheck = 0;
int aboveBreak = 0;
double baseline, alti;

//settings
int checkAlti = 300; // we passed 300, signal
int breakAlti = 1550; // breakoff, signal
int openAlti = 1000; // we are open, stop signaling

//colors
uint32_t red = strip.Color(255, 0, 0);
uint32_t green = strip.Color(0, 255, 0);
uint32_t blue = strip.Color(0, 0, 255);
uint32_t white = strip.Color(127, 127, 127);

void setup()
{
  Serial.begin(9600);

  if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  }
  baseline=getBaseline();
  Serial.println(baseline);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  flashStrip(green, 2,500); // Green, signal ready: 3 times 100ms delay
}

void loop() {
    alti=bmp.readAltitude(baseline);
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

    delay(1000);
}

// set a reference pressure, smooth it and use this as 0m
double getBaseline(){
  double bs;
  int numReadings = 10;

  double readings[numReadings];
  int readIndex = 0;
  double total = 0;
  double average = 0;
  int thisReading = 0;

  for (thisReading; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }

  readIndex = 0;
  thisReading = 0;
  //make 10 measurments and then return the average pressure
  for (thisReading; thisReading < numReadings; thisReading++) {

    readings[thisReading]=bmp.readPressure();
    total = total + readings[readIndex];
    readIndex = readIndex + 1;
    delay(100);
  }
  average = total / numReadings;
  bs=average/100;

  return bs;
}

void flashStrip(uint32_t color, int numTimes, int duration){
   for (int i = 0; i < numTimes; i++){
    fullColor(color);
    delay(duration);
    fullColor(strip.Color(0, 0, 0));
    delay(duration);
   }
}

void fullColor(uint32_t c) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
  }
}
