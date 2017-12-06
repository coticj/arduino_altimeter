// set a reference pressure, smooth it and use this as 0m
double getBaseline(){
  double bs;
  int numReadings = 10;

  double readings[numReadings]; 
  int readIndex = 0;
  double total = 0;
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

void flashStrip(uint32_t color, int numTimes, int duration){
   for (int i = 0; i < numTimes; i++){
    fullColor(color);
    delay(duration);
    fullColor(strip.Color(0, 0, 0));
    delay(duration);
   }   
}

void fullColor(uint32_t c) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();    
  }
}
