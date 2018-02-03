#define PIN 4
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_NeoPixel.h>
#include "FS.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

WebServer server(80);

Adafruit_BMP280 bmp; // I2C
Adafruit_NeoPixel strip = Adafruit_NeoPixel(4, PIN, NEO_GRB + NEO_KHZ800);
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

double altitude;

typedef struct
{
  unsigned long timeMillis;
  double altitude;
}  logEntry;

logEntry jumpLog[1200]; // 10 min odčitkov na pol sekunde

RTC_DATA_ATTR double baseline;
RTC_DATA_ATTR double pressureHistory[11];
RTC_DATA_ATTR int logIndex = 0;
RTC_DATA_ATTR bool firstBoot = true;
RTC_DATA_ATTR time_t sleepTimestamp;

bool startServer = false;
bool clientConnected = false;
time_t requestedTime = 0;
const char* host = "alti";

// Configuration that we'll store on disk
struct Config {
  char ssid[64];
  char password[64];
  char dz[64];
  char aircraft[64];
};

Config config;

void loadConfiguration(Config &config) {
  // Open file for reading
  File file = SPIFFS.open("/config.txt", "r");

  // Allocate the memory pool on the stack.
  // Don't forget to change the capacity to match your JSON document.
  // Use arduinojson.org/assistant to compute the capacity.
  const size_t bufferSize = JSON_OBJECT_SIZE(4) + 100;
  DynamicJsonBuffer jsonBuffer(bufferSize);

  // Parse the root object
  JsonObject &root = jsonBuffer.parseObject(file);

  if (!root.success())
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonObject to the Config
  strlcpy(config.ssid,                   // <- destination
          root["ssid"] | "alti-unset",  // <- source
          sizeof(config.ssid));          // <- destination's capacity
  strlcpy(config.password, root["password"] | "altiunset", sizeof(config.password));
  strlcpy(config.dz, root["dz"] | "Unknown", sizeof(config.dz));
  strlcpy(config.aircraft, root["aircraft"] | "Unknown", sizeof(config.aircraft));

  file.close();
}

void setup()
{
  Serial.begin(115200);
  Serial.println(F(""));
  if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  }

  // printStatus(); // altitude je še 0.00, ker se ga še ne potrebuje

  updatePressureHistory();
  ifNoChangeOnGroundStartDeepSleep();

  SPIFFS.begin();
  //naloži konfiguracijo
  Serial.println(F("Loading config..."));
  loadConfiguration(config);

  if (firstBoot) {

    strip.begin();
    flashStrip(off, 200, 0); // Initialize all pixels to 'off'

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

    Serial.print(F("reset count: "));
    Serial.println(resetCount);

    if (resetCount == 4) {
      flashStrip(green, 1, 1000, 0);

      WiFi.softAP(config.ssid, config.password);

      Serial.println();
      Serial.print(F("IP address: "));
      Serial.println(WiFi.softAPIP());

      httpServer();
      startServer = true;
      clientConnected = true;

      Serial.println(F("HTTP server started"));
    }
    else {
      File f = SPIFFS.open("/reset", "w");
      f.print(resetCount);
      f.close();
      flashStrip(orange, 1, 2000, 0); // Čaka. Če med tem resetiraš, se ohrani resetCount v datoteki, sicer se datoteka pobriše.
    }
    SPIFFS.remove("/reset");

    baseline = pressureHistory[logIndex];
    flashStrip(off, 200, 0);
    flashStrip(blue, 2, 500, 200, 500); // Signal OK
    signalBatteryPercentage();
  }
  else {
    strip.begin();
    flashStrip(off, 200, 0); // Initialize all pixels to 'off'
    long adjustment = intervalGround / 1000;
    setTime(sleepTimestamp);
    adjustTime(adjustment);
  }

  firstBoot = false;
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - requestedTime >= 60 * 1000) {
    clientConnected = false;
  }

  if (startServer) {
    server.handleClient();
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    altitude = getAltitude();

    // printStatus(); //odkomentiraj za debuging

    jumpLog[logIndex] = (logEntry) {
      millis(), altitude
    };

    updatePressureHistory();
    ifNoChangeOnGroundStartDeepSleep();

    // če še ni prosti pad, vpisuje samo zadnjih 11 meritev
    if (logIndex == 10 &&
        (jumpLog[logIndex - 10].altitude - jumpLog[logIndex].altitude) < 50) {
      int i = 0;
      for (i; i < 10; i++) {
        jumpLog[i] = jumpLog[i + 1];
      }
      --logIndex;
    }

    ++logIndex;

    // če se dvigne iz tal
    if (interval == intervalGround &&
        altitude > 50 &&
        altitudeChange(10)) {
      interval = intervalOffGround;
    }

    // če je nazaj na tleh po prostem padu
    if (interval == intervalOffGround &&
        logIndex > 60 &&
        altitude < 100 &&
        !altitudeChange(60)) {
      saveLog(30);

      logIndex = 0;
      interval = intervalGround;
      flashStrip(orange, 1, 1000);
    }
  }
}

void printStatus() {
  Serial.print(F("altitude: "));
  Serial.print(altitude);
  Serial.print(F(" m | logIndex: "));
  Serial.print(logIndex);
  Serial.print(F(" | pressure: "));
  Serial.print(getPressure() * 100);
  Serial.print(F(" Pa | temp: "));
  Serial.print(bmp.readTemperature());
  Serial.print(F(" °C | batt: "));
  Serial.print(getBatteryVoltage());
  Serial.print(F(" V; "));
  Serial.print(getBatteryPercentage());
  Serial.println(F(" %"));
}

void updatePressureHistory() {
  // zapisuje samo 11 meritev. Če zapis na indexu 10 že obstaja, premakne vse zapise za eno mesto naprej
  if ((pressureHistory[10] == 0)) {
    pressureHistory[logIndex] = getPressure();
  }
  else {
    int i;
    for (i; i < 10; i++) {
      pressureHistory[i] = pressureHistory[i + 1];
    }
    pressureHistory[10] = getPressure();
  }
}

void ifNoChangeOnGroundStartDeepSleep() {
  // če je na tleh in ni spremembe v zadnjih 10 zapisih, popravi baseline
  if (interval == intervalGround &&
      logIndex >= 10 &&
      !pressureAltitudeChange() &&
      !clientConnected) {
    baseline = pressureHistory[logIndex - 10];
    logIndex = 10;
    esp_sleep_enable_timer_wakeup(intervalGround * 1000); // milliseconds to microseconds
    sleepTimestamp = now();
    esp_deep_sleep_start();
  }
}

bool altitudeChange(int offset) {
  double delta = jumpLog[logIndex].altitude - jumpLog[logIndex - offset].altitude;
  return (delta < -2 || delta > 2);
}

bool pressureAltitudeChange() {
  double delta = bmp.readAltitude(pressureHistory[10]) - bmp.readAltitude(pressureHistory[0]);
  return (delta < -2 || delta > 2);
}

void saveLog(int ignoreLastEntries) {
  // write array to file
  // open file for writing
  File f = SPIFFS.open("/log.txt", "a");
  if (!f) {
    Serial.println(F("file open failed"));
  }
  else {
    Serial.println(F("====== Writing to SPIFFS file ========="));

    time_t timestamp = now();

    f.print(timestamp);
    f.print(F("|"));

    unsigned long timeFirst = jumpLog[0].timeMillis;
    int i = 0;
    for (i; i <= logIndex - ignoreLastEntries; i++) {
      unsigned long timeRelative = (jumpLog[i].timeMillis - timeFirst) / 1000; // milliseconds to seconds
      f.print(timeRelative);
      f.print(F(","));
      f.print(jumpLog[i].altitude);
      f.print(F(";"));
    }
    f.println(F(";"));
  }

  f.close();
}

void printLog() {
  File f = SPIFFS.open("/log.txt", "r");
  if (!f) {
    Serial.println(F("file open failed"));
  }
  else {
    String s;
    while (f.available()) {
      s += char(f.read());
    }
    f.close();
    int s_end = s.lastIndexOf(",");
    s.remove(s_end);
    Serial.println(s);
  }
}

double getAltitude() {
  return bmp.readAltitude(baseline);
}

double getPressure() {
  return bmp.readPressure() / 100;
}

double getTemperature() {
  return bmp.readTemperature();
}
