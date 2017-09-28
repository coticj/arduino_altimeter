int ledPin = 2;                
int aboveCheck = 0;
int aboveBreak = 0;
double baseline, alti;

//settings
int checkAlti = 300; // we passed 300, signal
int breakAlti = 1550; // breakoff, signal
int openAlti = 1000; // we are open, stop signaling

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

#define BMP_SCK 13
#define BMP_MISO 12
#define BMP_MOSI 11 
#define BMP_CS 10

Adafruit_BMP280 bmp; // I2C

void setup()
{
  Serial.begin(9600);
  
  if (!bmp.begin()) {  
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  }
  baseline=getBaseline();
  pinMode(ledPin, OUTPUT);      // sets the digital pin as output
  Serial.println(baseline);
  blinkLed(4,200); // turned on
}


void loop()
{
    alti=bmp.readAltitude(baseline);
    Serial.print(F("Curr alt = "));
    Serial.print(alti); //
    Serial.println(" m");
    Serial.println();
    
    if (alti > checkAlti && aboveCheck == 0) {
      aboveCheck = 1;
      blinkLed(3,200);
    }

    if (alti > breakAlti && aboveBreak == 0) {
      aboveBreak = 1;
      blinkLed(2,500);
    }

    if (alti < breakAlti && aboveBreak == 1) {
      blinkLed(20,200);
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

  double readings[numReadings];      // the readings from the analog input
  int readIndex = 0;              // the index of the current reading
  double total = 0;                  // the running total
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

void blinkLed(int numTimes, int duration){
   int i;
   for (i = 0; i < numTimes; i++){
    digitalWrite(ledPin, HIGH);
    delay(duration);
    digitalWrite(ledPin, LOW);
    delay(duration);
   }   
}
