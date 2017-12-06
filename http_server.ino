void httpServer()
{
  server.on ( "/battery", []() {
    server.send ( 200, "text/plain", String(getBatteryVoltage()));
  } );

  server.on ( "/all", []() {
    server.send ( 200, "text/plain", "{\"temp\":\""+String(getTemperature()) +"\",\"relalt\":\""+String(alti)+"\",\"baseline\":\""+String(baseline)+"\",\"batteryVoltage\":\""+ String(getBatteryVoltage()) +"\",\"batteryPercentage\":\""+String(getBatteryPercentage()) +"\"}");
  } );

  server.on ( "/temp", []() {
    server.send ( 200, "text/plain", String(getTemperature()));
  } );

  server.on ( "/pres", []() {
    server.send ( 200, "text/plain", String(getPressure()));
  } );

  server.on ( "/calibrate", []() {
    server.send ( 200, "text/plain", "ok");
    baseline = getBaseline();
  } );

  server.on ( "/woff", []() {
    server.send ( 200, "text/plain", "ok");
    btStop();
    WiFi.mode(WIFI_OFF);
  } );

  server.on ( "/restart", []() {
    server.send ( 200, "text/plain", "ok");
    ESP.restart();
  } );

  server.on ( "/test", []() {
    server.send ( 200, "text/plain", "ok");
    flashStrip(red, 4,200);
    flashStrip(green, 4,200);
    flashStrip(blue, 4,200);
    flashStrip(white, 4,200);
    flashStrip(orange, 4,200);
  } );

  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  MDNS.begin(host);
  server.begin();
  SPIFFS.begin();
}
