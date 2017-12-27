# Arduino altimeter

Visual alti works with neopixel led strip. Any arduino works fine. You will also need a [BMP280 module](http://www.ebay.com/itm/BMP280-Pressure-Sensor-Module-High-Precision-Atmospheric-Arduino-Replace-BMP180-/201538104303?epid=581281548&hash=item2eec9b6bef:g:rR8AAOSwu1VW4CQC) and a [Neopixel led strip](http://www.ebay.com/itm/1M-To-5M-WS2812B-LED-Strip-5050-SMD-RGB-Light-60-144-LEDs-M-IC-5V-Lighting-Lamp-/132311838859?var=&hash=item1ece66708b:m:mPRwIKhzgvWfpK_8GtCFFQw)

### Required libraries
* [Modified Adafruit BMP280 library](https://github.com/EquipeRocket/Adafruit_BMP280_Library)
* [Adafruit Neopixel library](https://github.com/adafruit/Adafruit_NeoPixel)
* [SPIFFS upload tool](https://github.com/esp8266/arduino-esp8266fs-plugin)
* [Webserver TNG](https://github.com/bbx10/WebServer_tng)

### Todo
* ground detection - use deep sleep when we are on the ground
* flight detection
* freefall detection - probably take two measurments 1s apart and if the altitude diff is more than 35m/s we are in freefall
