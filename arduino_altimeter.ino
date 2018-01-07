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
const int ledPin =  LED_BUILTIN;

//colors
uint32_t red = strip.Color(255, 0, 0);
uint32_t green = strip.Color(0, 255, 0);
uint32_t blue = strip.Color(0, 0, 255);
uint32_t yellow = strip.Color(255, 255, 0);
uint32_t orange = strip.Color(255, 127, 0);
uint32_t white = strip.Color(127, 127, 127);
uint32_t off = strip.Color(0, 0, 0);

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long intervalGround = 10000;           // interval at which to measure (milliseconds)
const long intervalOffGround = 500;           // interval at which to measure (milliseconds)

long interval = intervalGround;

void flashStrip(uint32_t color, int numTimes, int onDuration, int offDuration = -1, int finalDelay = -1);
void flashBuiltinLed(int numTimes, int onDuration, int offDuration = -1, int finalDelay = -1);

double altiLog[1200]; //10 min odčitkov na pol sekunde

unsigned long timeLog[1200]; //10 min odčitkov na pol sekunde
RTC_DATA_ATTR double baselineHistory[10];
RTC_DATA_ATTR int logIndex = 0;
RTC_DATA_ATTR bool firstBoot = true;

void setup()
{
  Serial.begin(115200);
  Serial.println("");
  if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  }

  int i;
  if (logIndex < 10) {
    baselineHistory[logIndex] = getBaseline();
  }
  else {
    for (i; i < 10; i++) {
      baselineHistory[i] = baselineHistory[i + 1];
    }
    baselineHistory[9] = getBaseline();
  }

  if (logIndex >= 10 &&
      !baselineAltiChange(10)) {
    baseline = baselineHistory[0];
    esp_sleep_enable_timer_wakeup(intervalGround * 1000); //micro seconds to milliseconds
    esp_deep_sleep_start();
  }
  else if (!firstBoot) {
    SPIFFS.begin();
    strip.begin();
    setStrip(off); // Initialize all pixels to 'off'
  }

  if (firstBoot) {

    strip.begin();
    setStrip(off); // Initialize all pixels to 'off'

    SPIFFS.begin();

    int resetCount = 1;
    if (SPIFFS.exists("/reset")) {
      File f = SPIFFS.open("/reset", "r");
      String s;
      while (f.available()) {
        s += char(f.read());
      }
      f.close();
      resetCount += s.toInt();
    }

    Serial.print("reset count: ");
    Serial.println(resetCount);

    if (resetCount == 4) {
      flashStrip(green, 1, 1000, 0);
      printLog();
    }
    else {
      File f = SPIFFS.open("/reset", "w");
      f.print(resetCount);
      f.close();
      flashStrip(orange, 1, 2000, 0); // čaka. če med tem resetiraš, se ohrani resetCount v datoteki, sicer se datoteka pobriše
    }
    SPIFFS.remove("/reset");

    baseline = getBaseline();
    flashStrip(off, 200, 0);
    flashStrip(blue, 2, 500, 200, 500); // Signal OK
    signalBatteryPercentage();
  }

  firstBoot = false;
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    alti = getAltitude();
    Serial.print(F("altitude: "));
    Serial.print(alti); //
    Serial.print(" m | logIndex: ");
    Serial.print(logIndex);
    Serial.print(" | pressure: ");
    Serial.print(baseline * 100);
    Serial.print(" Pa | temp: ");
    Serial.print(bmp.readTemperature());
    Serial.print(" °C | batt: ");
    Serial.print(getBatteryVoltage());
    Serial.print(" V; ");
    Serial.print(getBatteryPercentage());
    Serial.println(" %");

    altiLog[logIndex] = alti;
    timeLog[logIndex] = millis();

    // zapisuje samo zadnjih 10 meritev
    int i;
    if (logIndex < 10) {
      baselineHistory[logIndex] = getBaseline();
    }
    else {
      for (i; i < 10; i++) {
        baselineHistory[i] = baselineHistory[i + 1];
      }
      baselineHistory[9] = getBaseline();
    }

    // če je na tleh popravi nulo
    if (interval == intervalGround &&
        logIndex >= 10 &&
        !altiChange(10)) {
      baseline = baselineHistory[0];
      Serial.println("reset loop");
      esp_sleep_enable_timer_wakeup(intervalGround * 1000); //micro seconds to milliseconds
      esp_deep_sleep_start();
    }

    // če še ni prosti pad, vpisuje samo zadnjih 10 meritev
    if (logIndex == 10 &&
        (altiLog[logIndex - 10] - altiLog[logIndex]) < 50) {
      int i = 0;
      for (i; i < 10; i++) {
        altiLog[i] = altiLog[i + 1];
        timeLog[i] = timeLog[i + 1];
      }
      --logIndex;
    }

    ++logIndex;

    // če je na tleh
    if (interval == intervalGround &&
        alti > 50 &&
        altiChange(10)) {
      interval = intervalOffGround;
    }

    // nazaj na tleh
    if (interval == intervalOffGround &&
        logIndex > 60 &&
        alti < 100 &&
        !altiChange(60)) {
      saveLog(30);

      logIndex = 0;
      interval = intervalGround;
      flashStrip(orange, 1, 1000);
    }
  }
}

bool altiChange(int offset) {
  double delta = altiLog[logIndex] - altiLog[logIndex - offset];
  return (delta < -2 || delta > 2);
}

bool baselineAltiChange(int offset) { //zato da ohranimo podatke čez deep sleep
  double delta = bmp.readAltitude(baselineHistory[logIndex - 1]) - bmp.readAltitude(baselineHistory[logIndex - offset]);
  return (delta < -2 || delta > 2);
}

void saveLog(int ignoreLastEntries) {
  //write array to file
  // open file for writing
  File f = SPIFFS.open("/log.txt", "a");
  if (!f) {
    Serial.println("file open failed");
  }
  else {
    Serial.println("====== Writing to SPIFFS file =========");
    f.print("{ \"readings\": [ ");
    int i = 0;
    for (i; i <= logIndex - ignoreLastEntries; i++) {
      f.print("{\"time\": ");
      f.print(timeLog[i]);
      f.print(", ");
      f.print("\"altitude\": ");
      f.print(altiLog[i]);
      f.print("}");
      if (i < logIndex - ignoreLastEntries) {
        f.print(",");
      }
    }
    f.println(" ]");
    f.println(" }, ");
  }

  f.close();
}

void printLog() {
  File f = SPIFFS.open("/log.txt", "r");
  if (!f) {
    Serial.println("file open failed");
  }
  else {
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
  }
}

double getAltitude() {
  return bmp.readAltitude(baseline);
}

double getBaseline() {
  return bmp.readPressure() / 100;
}

void flashStrip(uint32_t color, int numTimes, int onDuration, int offDuration, int finalDelay) {
  if (offDuration == -1) {
    offDuration = onDuration;
  };
  for (int i = 0; i < numTimes; i++) {
    setStrip(color);
    delay(onDuration);
    setStrip(off);
    delay(offDuration);
  }
  if (finalDelay > 0) {
    delay(finalDelay);
  }
}

void flashBuiltinLed(int numTimes, int onDuration, int offDuration, int finalDelay) {
  if (offDuration == -1) {
    offDuration = onDuration;
  };
  for (int i = 0; i < numTimes; i++) {
    digitalWrite(ledPin, HIGH);
    delay(onDuration);
    digitalWrite(ledPin, LOW);
    delay(offDuration);
  }
  if (finalDelay > 0) {
    delay(finalDelay);
  }
}

void setStrip(uint32_t c) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
  }
}

float getBatteryVoltage()
{
  float measurement = analogRead(A13);

  float measuredvbat = (measurement / 4095) * 2 * 3.3 * 1.1;

  return measuredvbat;
}

float getBatteryPercentage()
{
  return _min(map(getBatteryVoltage() * 10, 3.30 * 10, 4.2 * 10, 0, 100), 100); // Calculate Battery Level (Percent)
}

void signalBatteryPercentage() {
  pinMode(ledPin, OUTPUT); //set builtin led
  float batt = getBatteryPercentage();
  if (batt < 20) {
    flashBuiltinLed(1, 2000); // no bars
  }
  else if (batt < 40) {
    flashBuiltinLed(1, 500, 0); // one bar
  }
  else if (batt < 60) {
    flashBuiltinLed(2, 500, 200); // two bars
  }
  else if (batt < 80) {
    flashBuiltinLed(3, 500, 200); // three bars
  }
  else {
    flashBuiltinLed(4, 500, 200); // four bars
  }
}


