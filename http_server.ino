void httpServer()
{
  server.on("/restart", []() {
    server.send (200, "text/plain", "ok");
    ESP.restart();
  } );

  server.on("/all", []() {
    server.send (200, "text/plain",
                 "{\"temp\":\"" + String(getTemperature()) +
                 "\",\"battery\":\"" + String(getBatteryPercentage()) +
                 "\",\"time\":\"" + String(now()) +
                 "\",\"location\":\"" + String(config.location) +
                 "\",\"aircraft\":\"" + String(config.aircraft) +
                 "\",\"lastJump\":\"" + String(config.lastJump) +
                 "\"}");
    requestedTime = millis();
  });

  server.on("/test", []() {
    server.send ( 200, "text/plain", "ok");
    flashStrip(red, 4, 200);
    flashStrip(green, 4, 200);
    flashStrip(blue, 4, 200);
    flashStrip(white, 4, 200);
    flashStrip(orange, 4, 200);
    requestedTime = millis();
  });

  server.on("/time", []() {
    if (server.hasArg("time")) {
      setTime(server.arg("time").toInt());
      server.send(200, "text/html", "Time was set");
      requestedTime = millis();
    }
  });

  server.on("/saveDetails", []() {
    if (server.hasArg("id") && server.hasArg("details")) {

      if (SPIFFS.exists("/log.txt")) {
        File fileLog = SPIFFS.open("/log.txt", "r");

        String logNew;
        while (fileLog.available()) {
          String s = fileLog.readStringUntil('\n');
          if (s.startsWith(server.arg("id") + "|")) {
            s.replace("||", "|/" + server.arg("details") + "||");
          }
          logNew += (s + '\n');
        }
        fileLog.close();

        fileLog = SPIFFS.open("/log.txt", "w");
        fileLog.print(logNew);
        fileLog.close();

        server.send(200, "text/html", "Details were saved");
        requestedTime = millis();
      }
    }
  });

  server.on("/clearLogs", []() {
    // get last ID
    int lastId = 0;
    if (SPIFFS.exists("/lastId")) {
      File fileLastId = SPIFFS.open("/lastId", "r");
      String s;
      while (fileLastId.available()) {
        s += char(fileLastId.read());
      }
      fileLastId.close();
      lastId = s.toInt() + 1;
    }

    // remove
    int i = 0;
    for (i; i <= lastId; i++) {
      String filename = "/logData_";
      filename.concat(i);
      filename.concat(".txt");
      SPIFFS.remove(filename);
    }

    SPIFFS.remove("/log.txt");
    SPIFFS.remove("/lastId");

    server.send(200, "text/html", "Logs were cleared.");
    requestedTime = millis();
  });

  server.on("/getConfig", []() {
    String s =  "{\"ssid\":\"" + String(config.ssid) +
                "\",\"password\":\"" + String(config.password) +
                "\",\"location\":\"" + String(config.location) +
                "\",\"aircraft\":\"" + String(config.aircraft) +
                "\",\"lastJump\":\"" + String(config.lastJump) +
                "\"}";
    server.send (200, "text/plain", s);
    requestedTime = millis();
  });

  server.on("/updateConfig", []() {
    saveConfiguration(server.arg("ssid"), server.arg("password"), server.arg("location"), server.arg("aircraft"), server.arg("lastJump"));
    loadConfiguration(config);
    server.sendHeader("Location", "/");
    server.send(301);
    requestedTime = millis();
  });

  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
      requestedTime = millis();
    }
  });

  MDNS.begin(host);
  server.begin();
}

//save config
void saveConfiguration(String ssid, String password, String location, String aircraft, String lastJump) {
  // Delete existing file, otherwise the configuration is appended to the file
  SPIFFS.remove("/config.txt");
  // Open file for writing
  File file = SPIFFS.open("/config.txt", "w");
  const size_t bufferSize = JSON_OBJECT_SIZE(4) + 100;
  DynamicJsonBuffer jsonBuffer(bufferSize);

  // Parse the root object
  JsonObject &root = jsonBuffer.createObject();

  // Set the values
  root["ssid"] = ssid;
  root["password"] = password;
  root["location"] = location;
  root["aircraft"] = aircraft;
  root["lastJump"] = lastJump;

  // Serialize JSON to file
  if (root.printTo(file) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  file.close();
}

//webserver
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}
//webserver
String getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

// webserver
bool handleFileRead(String path) {

  requestedTime = millis();

  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}
