//battery

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

//led

//void flashStrip(uint32_t color, int numTimes, int onDuration, int offDuration, int finalDelay) {
//  if (offDuration == -1) {
//    offDuration = onDuration;
//  };
//  for (int i = 0; i < numTimes; i++) {
//    setStrip(color);
//    delay(onDuration);
//    setStrip(off);
//    delay(offDuration);
//  }
//  if (finalDelay > 0) {
//    delay(finalDelay);
//  }
//}

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

//void setStrip(uint32_t c) {
//  for (uint16_t i = 0; i < strip.numPixels(); i++) {
//    strip.setPixelColor(i, c);
//  }
//  strip.show();
//}


