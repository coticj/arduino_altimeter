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

const int serverActiveAfterLastRequest = 60;  // seconds

void flashStrip(uint32_t color, int numTimes, int onDuration, int offDuration = -1, int finalDelay = -1);
void flashBuiltinLed(int numTimes, int onDuration, int offDuration = -1, int finalDelay = -1);

double altitude;

typedef struct
{
  unsigned long time;
  double altitude;
}  logEntry;

logEntry jumpLog[1200]; // 10 min odčitkov na pol sekunde

bool emulate = false;
int emulatorAltitude = 4600;
int emulatorLoopIndex = 0;

RTC_DATA_ATTR double baseline;
RTC_DATA_ATTR double pressureHistory[11];
RTC_DATA_ATTR int logIndex = 0;
RTC_DATA_ATTR bool firstBoot = true;
RTC_DATA_ATTR time_t sleepTimestamp = 0;

bool startServer = false;
bool clientConnected = false;
time_t clientLeaseTime = 0;
const char* host = "alti";

// Configuration that we'll store on disk
struct Config {
  char ssid[64];
  char password[64];
  char location[64];
  char aircraft[64];
  int lastJump;
};

Config config;

void loadConfiguration(Config &config) {
  // Open file for reading
  File fileConfig = SPIFFS.open("/config.txt", "r");

  // Allocate the memory pool on the stack.
  // Don't forget to change the capacity to match your JSON document.
  // Use arduinojson.org/assistant to compute the capacity.
  const size_t bufferSize = JSON_OBJECT_SIZE(4) + 100;
  DynamicJsonBuffer jsonBuffer(bufferSize);

  // Parse the root object
  JsonObject &root = jsonBuffer.parseObject(fileConfig);

  if (!root.success())
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonObject to the Config
  strlcpy(config.ssid,                   // <- destination
          root["ssid"] | "alti-unset",   // <- source
          sizeof(config.ssid));          // <- destination's capacity
  strlcpy(config.password, root["password"] | "altiunset", sizeof(config.password));
  strlcpy(config.location, root["location"] | "Unknown", sizeof(config.location));
  strlcpy(config.aircraft, root["aircraft"] | "Unknown", sizeof(config.aircraft));
  config.lastJump = String(root["lastJump"] | "0").toInt();

  fileConfig.close();
}

void setup()
{
  Serial.begin(115200);
  Serial.println(F(""));
  if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  }

  //printStatus(); // altitude je še 0.00, ker se ga še ne potrebuje

  SPIFFS.begin();

  //printLog();

  //naloži konfiguracijo
  Serial.println(F("Loading config ..."));
  loadConfiguration(config);

  if (!firstBoot) {
    long adjustment = intervalGround / 1000;
    setTime(sleepTimestamp);
    adjustTime(adjustment);
  }

  updatePressureHistory();
  ifNoChangeOnGroundStartDeepSleep();

  strip.begin();
  flashStrip(off, 200, 0); // Initialize all pixels to 'off'

  if (firstBoot) {
    int resetCount = 1;
    if (SPIFFS.exists("/reset")) {
      File fileReset = SPIFFS.open("/reset", "r");
      String s;
      while (fileReset.available()) {
        s += char(fileReset.read());
      }
      fileReset.close();
      resetCount += s.toInt();
    }

    Serial.print(F("reset count: "));
    Serial.println(resetCount);

    File f = SPIFFS.open("/reset", "w");
    f.print(resetCount);
    f.close();

    if (resetCount == 3) {
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
      uint32_t flashColor = orange;

      if (resetCount == 4) {
        emulate = true;
        flashColor = red;
        Serial.println(F("Emulator started."));
      }

      flashStrip(flashColor, 1, 2000, 0); // Čaka. Če med tem resetiraš, se ohrani resetCount v datoteki, sicer se datoteka pobriše.
    }
    SPIFFS.remove("/reset");

    baseline = pressureHistory[logIndex];
    flashStrip(off, 200, 0);
    flashStrip(blue, 2, 500, 200, 500); // Signal OK
    signalBatteryPercentage();
  }

  firstBoot = false;
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis > clientLeaseTime) {
    clientConnected = false;
  }

  if (startServer) {
    server.handleClient();
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    altitude = getAltitude();

    //printStatus(); //odkomentiraj za debuging

    jumpLog[logIndex] = (logEntry) {
      millis(), altitude
    };

    updatePressureHistory();
    ifNoChangeOnGroundStartDeepSleep();

    if (emulate && emulatorAltitude >= 25 && emulatorLoopIndex > 20) {
      emulatorAltitude -= 50;
    }

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
    if (emulate) {
      ++emulatorLoopIndex;
    }

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
      emulatorLoopIndex = 0;
      emulatorAltitude = 4600;
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
  int id = 0;

  // get last ID, add one
  if (SPIFFS.exists("/lastId")) {
    File fileLastId = SPIFFS.open("/lastId", "r");
    String s;
    while (fileLastId.available()) {
      s += char(fileLastId.read());
    }
    fileLastId.close();
    id = s.toInt() + 1;
  }

  ++config.lastJump;
  saveConfiguration(config.ssid, config.password, config.location, config.aircraft, String(config.lastJump));

  Serial.print(F("jump ID:"));
  Serial.println(id);

  // save basic data to log.txt
  File fileLog = SPIFFS.open("/log.txt", "a");
  Serial.println(F("====== Writing to log.txt ========="));
  time_t timestamp = now();
  fileLog.print(id);
  fileLog.print(F("|"));
  fileLog.print(config.lastJump);
  fileLog.print(F("|"));
  fileLog.print(timestamp);
  fileLog.print(F("|"));
  fileLog.print(config.location);
  fileLog.print(F("|"));
  fileLog.print(config.aircraft);
  fileLog.println(F("||"));
  fileLog.close();

  // save readings to new file in /logs folder
  String filename = "/logData_";
  filename.concat(id);
  filename.concat(".txt");
  File fileLogData = SPIFFS.open(filename, "w");
  String msg = "====== Writing to ";
  msg.concat(filename);
  msg.concat(" =========");
  Serial.println(msg);
  unsigned long timeFirst = jumpLog[0].time;
  int i = 0;
  for (i; i <= logIndex - ignoreLastEntries; i++) {
    float timeRelative = (float)(jumpLog[i].time - timeFirst) / 1000; // milliseconds to seconds
    fileLogData.print(timeRelative * 100);
    fileLogData.print(F(","));
    fileLogData.print(jumpLog[i].altitude * 100);
    if (i < (logIndex - ignoreLastEntries)) {
      fileLogData.print(F(";"));
    }
  }
  fileLogData.close();

  // save new id as last id
  File fileLastId = SPIFFS.open("/lastId", "w");
  fileLastId.print(id);
  fileLastId.close();
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
    Serial.println(s);
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

double getPressure() {
  return bmp.readPressure() / 100;
}

double getTemperature() {
  return bmp.readTemperature();
}
