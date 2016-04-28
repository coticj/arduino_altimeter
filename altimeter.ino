#include "BMP280.h"
#include "Wire.h"

BMP280 bmp;
#include "U8glib.h"

U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_NO_ACK|U8G_I2C_OPT_FAST);  
double baseline;

void setup()
{
  Serial.begin(9600);
  if(!bmp.begin()){
    Serial.println("BMP init failed!");
    while(1);
  }
  else Serial.println("BMP init success!");
  bmp.setOversampling(4);
  baseline=getBaseline();
  
  Serial.println(baseline);
  
}

void loop()
{
  double T,P;
  char result = bmp.startMeasurment();
  char dval[10];
  if(result!=0){
    delay(result);
    result = bmp.getTemperatureAndPressure(T,P);
    
      if(result!=0)
      {
        double A = bmp.altitude(P,baseline);
        
        Serial.print("T = \t");Serial.print(T,2); Serial.print(" degC\t");
        Serial.print("P = \t");Serial.print(P,2); Serial.print(" mBar\t");
        Serial.print("A = \t");Serial.print(A,0); Serial.println(" m");
        if (A > 1000)
        {
          dtostrf(A/1000, 1, 1, dval);
        }
        else if (A < 0)
        {
          dtostrf(0, 1, 0, dval);
        }
        else
        {
          dtostrf(A, 1, 0, dval);
        }
       
      }
      else {
        Serial.println("Error.");
      }
  }
  else {
    Serial.println("Error.");
  }
  u8g.firstPage();  
  do {
    u8g.setFont(u8g_font_fub49n);
  
    u8g.drawStr( 10, 55, dval);
  } while( u8g.nextPage() );
  
  delay(1000);
}

// set a reference pressure, smooth it and use this as 0m
double getBaseline(){
  double bs,T0;
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
    
    bmp.startMeasurment();
    bmp.getTemperatureAndPressure(T0,readings[thisReading] );
    total = total + readings[readIndex];
    readIndex = readIndex + 1;
    delay(100);
  }
  average = total / numReadings;
  bs=average;
  
  return bs;
}


