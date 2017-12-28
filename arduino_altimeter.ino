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

//colors
uint32_t red = strip.Color(255, 0, 0);
uint32_t green = strip.Color(0, 255, 0);
uint32_t blue = strip.Color(0, 0, 255);
uint32_t yellow = strip.Color(255, 255, 0);
uint32_t orange = strip.Color(255, 127, 0);
uint32_t white = strip.Color(127, 127, 127);

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long intervalGround = 5000;           // interval at which to measure (milliseconds)
const long intervalOffGround = 500;           // interval at which to measure (milliseconds)

long interval = intervalGround;

bool emulate = false;

int emulatorAltitude = 4600;

void flashStrip(uint32_t color, int numTimes, int onDuration, int offDuration = 0, int finalDelay = -1);

double altiLog[1200]; //10 min odčitkov na pol sekunde
unsigned long timeLog[1200]; //10 min odčitkov na pol sekunde
int altiLogIndex = 0;
int emulatorLoopIndex = 0;

double altiOffset = 0;

void setup()
{
  Serial.begin(9600);

  if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  }
  SPIFFS.begin();

  //print Log
//  File f = SPIFFS.open("/log.txt", "r");
//  String s;
//  while (f.available()) {
//    s += char(f.read());
//  }
//  f.close();
//  int s_end = s.lastIndexOf(",");
//  s.remove(s_end);
//  Serial.println("[");
//  Serial.println(s);
//  Serial.println("]");

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
    Serial.print(F("Altitude: "));
    Serial.print(alti); //
    Serial.print(" m | altiLogIndex: ");
    Serial.print(altiLogIndex);
    Serial.print(" | altiOffset: ");
    Serial.println(altiOffset);

    //    if (alti > 10) {
    //      Serial.print(millis());
    //      Serial.println(" ms");
    //    }

    //    if (!emulate) {
    //    if (alti > checkAlti && aboveCheck == 0) {
    //      aboveCheck = 1;
    //      flashStrip(blue, 3, 200); //we passed 300m, flash
    //    }
    //
    //    if (alti > openAlti && aboveOpen == 0) {
    //      aboveOpen = 1;
    //      flashStrip(white, 2, 200); //we passed 300m, flash
    //    }
    //
    //    if (alti > breakAlti && aboveBreak == 0) {
    //      aboveBreak = 1;
    //      flashStrip(white, 2, 500); //we passed breakoff altitude, flash
    //    }
    //
    //    if (alti < breakAlti && aboveBreak == 1) {
    //      flashStrip(red, 10, 200);
    //      aboveBreak = 0;
    //    }
    //
    //    if (alti < openAlti && aboveOpen == 1) {
    //      flashStrip(red, 10, 200);
    //      aboveOpen = 0;
    //    }
    //
    //    if (alti < checkAlti && aboveCheck == 1) {
    //      aboveCheck = 0;
    //    }
    //    }

    //    // modra za 1000 m, oranžna za 500 m
    //    if (int(alti) % 500 == 0 && alti > 0) {
    //      flashStrip(red, 1, 100, 300); // kratek rdeč za pozornost?
    //      flashStrip(blue, int(alti) / 1000, 300, 100, 300);
    //      if (int(alti) % 1000 == 500) {
    //        flashStrip(orange, 1, 100);
    //      }
    //    }

    //    if (emulate && emulatorAltitude >= 25 && emulatorLoopIndex > 20) {
    //      emulatorAltitude -= 50;
    //    }

    altiLog[altiLogIndex] = alti;
    timeLog[altiLogIndex] = millis();

    if (altiLogIndex == 10) {
      // če je na tleh popravi nulo
      if (interval == intervalGround &&
          !altiChange(10)) {
        altiOffset = altiLog[altiLogIndex - 10];
      }
      // če še ni prosti pad, vpisuje samo zadnjih 10 meritev
      if ((altiLog[altiLogIndex - 10] - altiLog[altiLogIndex]) < 100) {
        int i = 0;
        for (i; i < 10; i++) {
          altiLog[i] = altiLog[i + 1];
          timeLog[i] = timeLog[i + 1];
        }
        altiLogIndex--;
      }
    }

    altiLogIndex++;
    //    emulatorLoopIndex++;

    // če je na tleh
    if (interval == intervalGround &&
        alti > 50 &&
        altiChange(10)) {
      interval = intervalOffGround;
    }

    //na tleh
    if (interval == intervalOffGround &&
        altiLogIndex > 60 &&
        alti < 100 &&
        !altiChange(60)) {
      saveLog();
      //      f = SPIFFS.open("/f.txt", "r");

      //      String s;
      //      while (f.available()) {
      //        s += char(f.read());
      //      }
      //      f.close();
      //      int s_end = s.lastIndexOf(",");
      //      s.remove(s_end);
      //      Serial.println("[");
      //      Serial.println(s);
      //      Serial.println("]");
      //      delay(5000);

      altiLogIndex = 0;
      interval = intervalGround;
      //      emulatorLoopIndex = 0;
      //      emulatorAltitude = 4600;
      flashStrip(orange, 1, 1000);
    }
  }
}

bool altiChange(int offset) {
  double delta = altiLog[altiLogIndex] - altiLog[altiLogIndex - offset];
  return (delta < -2 || delta > 2);
}

void saveLog() {
  //write array to file
  // open file for writing
  File f = SPIFFS.open("/log.txt", "a");
  if (!f) {
    Serial.println("file open failed");
  }
  Serial.println("====== Writing to SPIFFS file =========");
  f.print("{ \"readings\": [ ");
  int i = 0;
  for (i; i <= altiLogIndex; i++) {
    f.print("{\"time\": ");
    f.print(timeLog[i]);
    f.print(", ");
    f.print("\"altitude\": ");
    f.print(altiLog[i]);
    f.print("}");
    if (i < altiLogIndex) {
      f.print(",");
    }
  }
  f.println(" ]");
  f.println(" }, ");

  f.close();
}

double getAltitude() {
  if (emulate) {
    return emulatorAltitude;
  }
  else {
    return bmp.readAltitude(baseline) - altiOffset;
  }
}

// set a reference pressure, smooth it and use this as 0m
double getBaseline() {
  double bs;
  int numReadings = 10;

  double readings[numReadings];
  int i = 0;
  double total = 0;
  double average = 0;

  for (i; i < numReadings; i++) {
    readings[i] = 0;
  }

  i = 0;
  //make 10 measurments and then return the average pressure
  for (i; i < numReadings; i++) {
    readings[i] = bmp.readPressure();
    total = total + readings[i];
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
