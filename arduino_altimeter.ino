#include <Arduino.h>
#include <U8g2lib.h>

#include <SPI.h>
#include <Wire.h>
#include <MS5611.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
U8G2_ST7565_NHD_C12864_F_4W_SW_SPI led(U8G2_R0, /* clock=*/ 17, /* data=*/ 21, /* cs=*/ 18, /* dc=*/ 16, /* reset=*/ 19);

MS5611 ms5611;
Adafruit_BMP280 bmp;

double referencePressureBMP;
double referencePressureMS;

void setup(void) {

  Serial.begin(115200);
  while (!ms5611.begin())
  {
    Serial.println("Could not find a valid MS5611 sensor, check wiring!");
    delay(500);
  }

  if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  }

  // Get reference pressure for relative altitude
  referencePressureMS = ms5611.readPressure();
  referencePressureBMP = bmp.readPressure();

  oled.begin();
  oled.setFlipMode(1);
  led.begin();
  led.setFlipMode(1);
}

void loop(void) {
  long realPressure = ms5611.readPressure();
  float relativeAltitude = ms5611.getAltitude(realPressure, referencePressureMS);

  long pressure = bmp.readPressure();
  float rleativeBMP = ms5611.getAltitude(pressure, referencePressureBMP);

  char altiMS[8];
  dtostrf(relativeAltitude, 3, 1, altiMS);

  char altiBMP[8];
  dtostrf(rleativeBMP, 3, 1, altiBMP);
  Serial.print("BMP: ");
  Serial.print(pressure);

  Serial.print(" MS: ");
  Serial.println(realPressure);

  oled.clearBuffer();          // clear the internal memory
  oled.setFont(u8g2_font_courB24_tn); // choose a suitable font
  oled.drawStr(15, 57, altiMS); // write something to the internal memory
  oled.sendBuffer();          // transfer internal memory to the display
  led.clearBuffer();          // clear the internal memory
  led.setFont(u8g2_font_courB24_tn); // choose a suitable font
  led.drawStr(15, 57, altiBMP); // write something to the internal memory
  led.sendBuffer();          // transfer internal memory to the display
  delay(1000);
}
