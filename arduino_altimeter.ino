#define PIN 4
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_NeoPixel.h>
#include "FS.h"
#include <SPIFFS.h>

Adafruit_BMP280 bmp; // I2C
Adafruit_NeoPixel strip = Adafruit_NeoPixel(4, PIN, NEO_GRB + NEO_KHZ800);
int aboveCheck = 0;
int aboveBreak = 0;
int aboveOpen = 0;
double baseline, alti;

//settings
int checkAlti = 300; // we passed 300, signal
int breakAlti = 1550; // breakoff, signal
int openAlti = 1000; // we are open, stop signaling

//color correction
extern const uint8_t gamma8[];

//colors
uint32_t red = strip.Color(255, 0, 0);
uint32_t green = strip.Color(0, 255, 0);
uint32_t blue = strip.Color(0, 0, 255);
uint32_t yellow = strip.Color(255, 255, 0);
uint32_t orange = strip.Color(255, 127, 0);
uint32_t white = strip.Color(127, 127, 127);
uint32_t realOrange = strip.Color(pgm_read_byte(&gamma8[255]), pgm_read_byte(&gamma8[165]), pgm_read_byte(&gamma8[0]));

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 500;           // interval at which to measure (milliseconds)

bool emulate = true;

int emulatorAltitude = 4600;

void flashStrip(uint32_t color, int numTimes, int onDuration, int offDuration = 0, int finalDelay = -1);

double altiReadings[1200]; //10 min odčitkov na pol sekunde
int altiReadingsIndex = 0;
int emulatorLoopIndex = 0;

void setup()
{
  Serial.begin(9600);

  if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  }
  SPIFFS.begin();

  baseline = getBaseline();
  Serial.println(baseline);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  flashStrip(green, 2, 500, 200); // Signal OK
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    alti = getAltitude();
    Serial.print(F("Curr alt = "));
    Serial.print(alti); //
    Serial.print(" m (");
    Serial.print(altiReadingsIndex);
    Serial.println(")");
    Serial.println();

    if (alti > 10) {
      Serial.print(millis());
      Serial.println(" ms");
    }

    if (!emulate) {
      if (alti > checkAlti && aboveCheck == 0) {
        aboveCheck = 1;
        flashStrip(blue, 3, 200); //we passed 300m, flash
      }

      if (alti > openAlti && aboveOpen == 0) {
        aboveOpen = 1;
        flashStrip(white, 2, 200); //we passed 300m, flash
      }

      if (alti > breakAlti && aboveBreak == 0) {
        aboveBreak = 1;
        flashStrip(white, 2, 500); //we passed breakoff altitude, flash
      }

      if (alti < breakAlti && aboveBreak == 1) {
        flashStrip(red, 10, 200);
        aboveBreak = 0;
      }

      if (alti < openAlti && aboveOpen == 1) {
        flashStrip(red, 10, 200);
        aboveOpen = 0;
      }

      if (alti < checkAlti && aboveCheck == 1) {
        aboveCheck = 0;
      }
    }

    // modra za 1000 m, oranžna za 500 m
    if (int(alti) % 500 == 0 && alti > 0) {
      flashStrip(red, 1, 100, 300); // kratek rdeč za pozornost?
      flashStrip(blue, int(alti) / 1000, 300, 100, 300);
      if (int(alti) % 1000 == 500) {
        flashStrip(orange, 1, 100);
      }
    }

    if (emulate && emulatorAltitude >= 25 && emulatorLoopIndex > 20) {
      emulatorAltitude -= 50;
    }

    altiReadings[altiReadingsIndex] = alti;

    // preveri, če še ni prosti pad
    if (altiReadingsIndex == 10 &&
        altiReadings[altiReadingsIndex] == altiReadings[altiReadingsIndex - 10]) {
      int i = 0;
      for (i; i < 10; i++) {
        altiReadings[i] = altiReadings[i + 1];
      }
      altiReadingsIndex--;
    }

    altiReadingsIndex++;
    emulatorLoopIndex++;

    // preveri, če je na tleh
    if (altiReadingsIndex > 20 &&
        altiReadings[altiReadingsIndex] == altiReadings[altiReadingsIndex - 10] && alti < 100) {
      //write array to file
      // open file for writing
      File f = SPIFFS.open("/f.txt", "a");
      if (!f) {
        Serial.println("file open failed");
      }
      Serial.println("====== Writing to SPIFFS file =========");
      f.print("{ \"interval\": ");
      f.print(interval);
      f.println(", ");
      f.print("\"altitudes\": [ ");
      int i = 0;
      for (i; i <= altiReadingsIndex; i++) {
        f.print(altiReadings[i]);
        if (i < altiReadingsIndex) {
          f.print(", ");
        }
      }
      f.println(" ]");
      f.println(" }, ");

      f.close();
      f = SPIFFS.open("/f.txt", "r");

      String s;
      while (f.available()) {
        s += char(f.read());
      }
      f.close();
      int s_end = s.lastIndexOf(",");
      s.remove(s_end);
      Serial.println("[");
      Serial.println(s);
      Serial.println("]");
      delay(5000);

      altiReadingsIndex = 0;
      emulatorLoopIndex = 0;
      emulatorAltitude = 4600;
      flashStrip(orange, 1, 1000);
    }
  }
}

double getAltitude() {
  if (emulate) {
    return emulatorAltitude;
  }
  else {
    return bmp.readAltitude(baseline);
  }
}

// set a reference pressure, smooth it and use this as 0m
double getBaseline() {
  double bs;
  int numReadings = 10;

  double readings[numReadings];
  int readIndex = 0;
  double total = 0;
  double average = 0;

  for (readIndex; readIndex < numReadings; readIndex++) {
    readings[readIndex] = 0;
  }

  readIndex = 0;
  //make 10 measurments and then return the average pressure
  for (readIndex; readIndex < numReadings; readIndex++) {
    readings[readIndex] = bmp.readPressure();
    total = total + readings[readIndex];
    delay(100);
  }
  average = total / numReadings;
  bs = average / 100;

  return bs;
}

void flashStrip(uint32_t color, int numTimes, int onDuration, int offDuration, int finalDelay) {
  if (offDuration == -1) {
    offDuration = onDuration;
  };
  for (int i = 0; i < numTimes; i++) {
    fullColor(color);
    delay(onDuration);
    turnLightsOff();
    delay(offDuration);
  }
  if (finalDelay > 0) {
    delay(finalDelay);
  }
}

void fullColor(uint32_t c) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
  }
}

void turnLightsOff() {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
    strip.show();
  }
}
