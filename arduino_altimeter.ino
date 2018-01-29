#define PIN 4
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_NeoPixel.h>
#include "FS.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

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
double altitudeLog[1200]; // 10 min odčitkov na pol sekunde
unsigned long timeLog[1200]; // 10 min odčitkov na pol sekunde

RTC_DATA_ATTR double baseline;
RTC_DATA_ATTR double pressureHistory[11];
RTC_DATA_ATTR int logIndex = 0;
RTC_DATA_ATTR bool firstBoot = true;

bool startServer = false;
const char* host = "alti-jure";

// Configuration that we'll store on disk
struct Config {
  char ssid[64];
  char password[64];
};

Config config;

void loadConfiguration(Config &config) {
  // Open file for reading
  File file = SPIFFS.open("/config.txt", "r");

  // Allocate the memory pool on the stack.
  // Don't forget to change the capacity to match your JSON document.
  // Use arduinojson.org/assistant to compute the capacity.
  const size_t bufferSize = JSON_OBJECT_SIZE(1) + 20;
  DynamicJsonBuffer jsonBuffer(bufferSize);

  // Parse the root object
  JsonObject &root = jsonBuffer.parseObject(file);

  if (!root.success())
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonObject to the Config
  strlcpy(config.ssid,                   // <- destination
          root["ssid"] | "alti-new",  // <- source
          sizeof(config.ssid));          // <- destination's capacity
  strlcpy(config.password, root["password"] | "altinew", sizeof(config.password));

  file.close();
}

void setup()
{
  Serial.begin(115200);
  Serial.println("");
  if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  }

  // printStatus(); // altitude je še 0.00, ker se ga še ne potrebuje

  updatePressureHistory();
  ifNoChangeOnGroundStartDeepSleep();

  if (firstBoot) {

    strip.begin();
    flashStrip(off, 200, 0); // Initialize all pixels to 'off'

    SPIFFS.begin();

    //naloži konfiguracijo
    Serial.println(F("Loading config..."));
    loadConfiguration(config);

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

      WiFi.softAP(config.ssid, config.password);

      Serial.println();
      Serial.print("IP address: ");
      Serial.println(WiFi.softAPIP());

      httpServer();
      startServer = true;

      Serial.println("HTTP server started");
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

    SPIFFS.begin();
  }

  firstBoot = false;
}

void loop() {
  unsigned long currentMillis = millis();

  if (startServer) {
    server.handleClient();
  }
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    altitude = getAltitude();

    //printStatus(); //odkomentiraj za debuging

    altitudeLog[logIndex] = altitude;
    timeLog[logIndex] = millis();

    updatePressureHistory();
    ifNoChangeOnGroundStartDeepSleep();

    // če še ni prosti pad, vpisuje samo zadnjih 11 meritev
    if (logIndex == 10 &&
        (altitudeLog[logIndex - 10] - altitudeLog[logIndex]) < 50) {
      int i = 0;
      for (i; i < 10; i++) {
        altitudeLog[i] = altitudeLog[i + 1];
        timeLog[i] = timeLog[i + 1];
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
      !pressureAltitudeChange()) {
    baseline = pressureHistory[logIndex - 10];
    logIndex = 10;
    esp_sleep_enable_timer_wakeup(intervalGround * 1000); //micro seconds to milliseconds
    esp_deep_sleep_start();
  }
}

bool altitudeChange(int offset) {
  double delta = altitudeLog[logIndex] - altitudeLog[logIndex - offset];
  return (delta < -2 || delta > 2);
}

bool pressureAltitudeChange() {
  double delta = bmp.readAltitude(pressureHistory[10]) - bmp.readAltitude(pressureHistory[0]);
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
      f.print(altitudeLog[i]);
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

double getPressure() {
  return bmp.readPressure() / 100;
}

double getTemperature() {
  return bmp.readTemperature();
}






