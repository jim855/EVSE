#include <Arduino.h>
#include <SPIFFS.h>
#include <define.h>

extern Setting setting;

String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ; 
}


unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

void writeConfig() {
  SPIFFS.begin(true);
  File settingFile = SPIFFS.open("/setting", "wb");
  settingFile.write((byte*) &setting, sizeof(setting));
  settingFile.close();
  SPIFFS.end();
}