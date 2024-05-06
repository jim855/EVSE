#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <openevse.h>
#include <Adafruit_GFX.h>
#include <Adafruit_RA8875.h>
#include <SoftwareSerial.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WebServer_ESP32_SC_W5500.h>
#include <uri/UriBraces.h>
#include <uri/UriRegex.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
// 自己管的部分
#include <MFRC522_I2C.h>
#include <buzzer.h>
#include <screen.h>
#include <define.h>
#include <utils.h>

RapiSender rapiSender(&RAPI_PORT);
Screen scr = Screen(LCD_CS, LCD_RST, LCD_SCK, LCD_MISO, LCD_MOSI);
Setting setting;
String eth_ip, eth_mac;
MFRC522 mfrc522(0x28, 0x28);
File RFID_DATA;
Buzzer buzzer = Buzzer(BEEPER,PWMCHANNEL,RESOLUTION);
unsigned long lock_countdown=0;
uint32_t next_status = 0;

double lastVolts=-1, lastAmps=-1, lastWalts=-1;
   
unsigned long epochValidUntil=0;

WebServer server(80);

void IRAM_ATTR EM() {
    // rapiSender.sendCmd("$FD");
    // log_e("SHUT DOWN EVSE!!!!");
}

void apiHandleOn() {
  rapiSender.sendCmd("$FE");
  server.send(200, F("application/json"), F("{ \"eves_status\": \"On\" }"));
}

void apiHandleOff() {
  rapiSender.sendCmd("$FD");
  server.send(200, F("application/json"), F("{ \"eves_status\": \"Off\" }"));

}

void apiHandleInfo() {

}

void apiAuth() {
  String cardUuid = server.pathArg(0);
  log_e("Authen by CardID: %s", cardUuid);
  rapiSender.sendCmd("$S4 0");
  server.send(200, F("application/json"), F("{ \"auth_status\": \"Unlocked\" }"));
  epochValidUntil = getTime() + 180;
}

void apiUnauth() {
  String cardUuid = server.pathArg(0);
  log_e("Authen by CardID: %s", cardUuid);
  rapiSender.sendCmd("$S4 1");
  epochValidUntil = 0;
  server.send(200, F("application/json"), F("{ \"auth_status\": \"Locked\" }"));
}

void apiHandleGetAmps() {
  double curAmps = lastAmps;
  String output = " {\"amps\":" + String(curAmps) +  "}";
  server.send(200, F("application/json"), output);
}

void apiHandleGetVolts() {
  double curVolts = lastVolts;
  String output = " {\"volts\":" + String(curVolts) +  "}";
  server.send(200, F("application/json"), output);
}

void apiHandleAmp() {
  //String user = server.pathArg(0);
  String amps = server.pathArg(0);
  log_e("Turn Amp to %s", amps);
  String cmd = "$SC "+ amps + " V";
  rapiSender.sendCmd(cmd);
  server.send(200, F("application/json"), F("{ \"message\": \"done\" }"));
}

void apiHandleClearConfig() {
  SPIFFS.begin(true);
  if (SPIFFS.exists("/setting")) {
    SPIFFS.remove("/setting");
  }
  SPIFFS.end();
  server.send(200, F("application/json"), F("{ \"message\": \"done\" }"));
}

void apiHandleListTags() {
  String tags[10];
  int cnt=0;
  // [ "aaa", "bbb" ]
  for (int x=0; x<10; x++) {
    if (setting.validTag[x].length() == 0) {
      log_e("[ListTag] Tag %d is empty", x);
    } else {
      log_e("[ListTag] Tag %d is %s", x, setting.validTag[x]);
      tags[cnt] = "\"" + setting.validTag[x] + "\"";
    }
  }
  String result= "[";
  for (int x=0; x<cnt; x++) {
    result = result + tags[cnt];
    if (x!=cnt-1) {
      result = result + ",";
    }
  }
  result = result + "]";
  server.send(200, F("application/json"), result);
}

void apiHandleAddTag() {
  String uuid = server.pathArg(0);
  int realCnt=0;
  for (int x=0; x<10; x++) {
    if (setting.validTag[x].length() == 0) {
      log_e("[ListTag] Tag %d is empty", x);
      setting.validTag[x] = uuid;
      break;
    }
    realCnt++;
  }
  writeConfig();
  server.send(200, F("application/json"), F("{ \"message\": \"done\" }"));
}

void apiHandleDelTag() {
  String uuid = server.pathArg(0);
  for (int x=0; x<10; x++) {
    if (setting.validTag[x].length() != 0) {
      if (setting.validTag[x] == uuid) {
        setting.validTag[x]="";
      }
    }
  }
  writeConfig();
  server.send(200, F("application/json"), F("{ \"message\": \"done\" }"));
}

void apiHandleSetName() {
  String name = server.pathArg(0);
  setting.name = name;
  writeConfig();
  server.send(200, F("application/json"), F("{ \"message\": \"done\" }"));
}

void setup()
{
  Serial.begin(115200);
  Serial1.begin(ATMEGA32_BAUD, SERIAL_8N1, ATMEGA32_RX, ATMEGA32_TX);
  while (!Serial && millis() < 5000);
    delay(500);
  scr.begin(RA8875_480x272);

  rapiSender.sendCmd("$FE");
  rapiSender.sendCmd("$S4 1");

  scr.bootDrawFrame();
  scr.bootDrawStatu("設定腳位");
  vTaskDelay(1000);
  pinMode(LED1,OUTPUT);
  pinMode(LED2,OUTPUT);
  pinMode(LED3,OUTPUT);
  digitalWrite(LED1,HIGH);
  digitalWrite(LED2,HIGH);
  digitalWrite(LED3,HIGH);

  scr.bootDrawStatu("設定揚聲器");
  vTaskDelay(1000);
  buzzer.begin();
  buzzer.Success();
  
  int count = 0;
  scr.bootDrawStatu("設定序列通訊");
  vTaskDelay(1000);
  Wire.begin(RFID_SDA, RFID_SCL);
  log_e("[Setup] Starting I2C scan:");
  for (byte i = 0; i < 128; i++)
  {
    Wire.beginTransmission(i);       // Begin I2C transmission Address (i)
    byte error = Wire.endTransmission();
    if (error == 0) // Receive 0 = success (ACK response)
    {
      log_e("[Setup] \tFound address: %d", i);
      count++; 
    }
  }
  log_e("[Setup] \tFound %d devices(s).", count);
  
  scr.bootDrawStatu("設定RFID");
  vTaskDelay(1000);
  mfrc522.PCD_Init();

  ///////////////////////////////////////////////////////////
  // if (mfrc522.PCD_PerformSelfTest()) {
  //   log_e("RFID Self Test pass");
  // } else {
  //   log_e("RFID Self Test failed");
  // }
  // mfrc522.PCD_AntennaOn();
  
  // log_e("RFID Antenna GAIN: %d", mfrc522.PCD_GetAntennaGain());
  // //////////////////////////////////////////////////////////////

  digitalWrite(LED1,LOW);
  digitalWrite(LED2,LOW);
  
  scr.bootDrawStatu("設定網路卡");
  vTaskDelay(1000);
  uint64_t chipId = ESP.getEfuseMac();
  byte mac[] = {0xDE, 0xAD, 0xBE, 0, 0, 0};
  log_e("[Setup] ESP32 WiFi Mac: %012llx", chipId);
  mac[5] = 0xFF & (chipId >> 24);
  mac[4] = 0xFF & (chipId >> 32);
  mac[3] = 0xFF & (chipId >> 40);
  log_e("[Setup] W5500 Mac: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  log_e("[Setup] W5500 Init");
  
  ESP32_W5500_onEvent();
  log_e("[Setup] W5500 onEvent done");
  ETH.begin(ETH_MISO, ETH_MOSI, ETH_SCK, ETH_CS, ETH_INT, 25, 2, mac);
  char hostname[12];
  sprintf(hostname, "EVSE_%02x%02x%02x", mac[3], mac[4], mac[5]);
  log_e("[Setup] Hostname: %s", hostname);
  ETH.setHostname(hostname);
  log_e("[Setup] W5500 begin done");

  scr.bootDrawStatu("取得IP....");
  vTaskDelay(1000);
  ESP32_W5500_waitForConnect();
  eth_ip = IpAddress2String(ETH.localIP());
  eth_mac = ETH.macAddress();

  log_e("[Setup] W5500 Final Status");
  log_e("[Setup] \tIP Addr: %s", IpAddress2String(ETH.localIP()));
  log_e("[Setup] \tSubnet: %s", IpAddress2String(ETH.subnetMask()));
  log_e("[Setup] \tGateway: %s", IpAddress2String(ETH.gatewayIP()));
  log_e("[Setup] \tDNS: %s", IpAddress2String(ETH.dnsIP()));

  scr.bootDrawStatu("起始網路對時");
  configTime(28800, 3600, "time.stdtime.gov.tw");
  vTaskDelay(1000);


  scr.bootDrawStatu("設定檔案系統");
  vTaskDelay(1000);
  log_e("[Setup] SPIFFS Init");
  if (!SPIFFS.begin(true)) {
    log_e("[Setup] SPIFFS failed");
    return;
  }
  log_e("[Setup] SPIFFS Inited");
  

  scr.bootDrawStatu("讀取設定值");
  vTaskDelay(1000);
  if (!SPIFFS.exists("/setting")) {
    // First time;
    log_e("[Setup] Init setting files");
    char buf[5];
    setting.name = buf;
    //setting.name = hostname;
    File settingFile = SPIFFS.open("/setting", "wb+");
    if (!settingFile) {
      log_e("[Setup] Setting file open failed");
      return;
    }
    settingFile.write((byte*) &setting, sizeof(setting));
    settingFile.close();
  } 
    log_e("[Setup] Read setting file");
    File settingFile = SPIFFS.open("/setting", "rb");

    settingFile.readBytes((char*) &setting, sizeof(setting));
    settingFile.close();

    log_e("[Setup] Hostname: %s", setting.name);
  
  SPIFFS.end();
  

  
  scr.bootDrawStatu("起始服務綁定");
  vTaskDelay(1000);

  server.on(F("/turnOff"), apiHandleOff);
  server.on(F("/turnOn"), apiHandleOn);
  server.on(F("/info"), apiHandleInfo);

  server.on(UriBraces("/auth/{}"), apiAuth);
  server.on(F("/unauth"), apiUnauth);

  server.on(F("/getAmps"), apiHandleGetAmps);
  server.on(F("/getVolts"), apiHandleGetVolts);

  server.on(UriBraces("/amp/{}"), apiHandleAmp);
  server.on(F("/listTags"), apiHandleListTags);
  server.on(UriBraces("/addTag/{}"), apiHandleAddTag);
  server.on(UriBraces("/delTag/{}"), apiHandleDelTag);
  server.on(UriBraces("/setName/{}"), apiHandleSetName);
  server.on(F("/clearConfig"), apiHandleClearConfig);
  server.begin();

  

  scr.bootDrawStatu("起始緊急開關綁定");
  vTaskDelay(1000);
  pinMode(BUTTON_1, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(BUTTON_1), EM, RISING);
  

  scr.bootDrawDone();
  vTaskDelay(2000);
  // Normal mode
  scr.normalDrawFrame(eth_mac, eth_ip, setting.name);
  buzzer.Success();
  //mfrc522.PCD_Reset();
}

void loop()
{
  server.handleClient();
  rapiSender.loop();

  if(millis() > next_status)
  {
    next_status = millis() + POLL_TIME;
    if(OpenEVSE.isConnected())
    {

      OpenEVSE.getStatus([](int ret, uint8_t evse_state, uint32_t session_time, uint8_t pilot_state, uint32_t vflags)
      {
        if(RAPI_RESPONSE_OK == ret)
        {
          if(evse_state==OPENEVSE_STATE_CHARGING){
            digitalWrite(LED2,HIGH);
          }
          else
          {
            digitalWrite(LED2,LOW);
          }
          String state_msg;
          bool failed = false;
          switch (evse_state) {
            case OPENEVSE_STATE_STARTING:
              state_msg = "開機中...."; break;
            case OPENEVSE_STATE_NOT_CONNECTED:
              state_msg = "車輛未連接"; break;
            case OPENEVSE_STATE_CONNECTED:
              state_msg = "車輛已連接"; break;
            case OPENEVSE_STATE_CHARGING:
              state_msg = "車輛充電中"; break;
            case OPENEVSE_STATE_VENT_REQUIRED:
              state_msg = "車輛需要放電"; break;
            case OPENEVSE_STATE_DIODE_CHECK_FAILED:
              state_msg = "Diode檢查失敗";
              failed = true;
              break;
            case OPENEVSE_STATE_GFI_FAULT:
              state_msg = "GFI檢查失敗";
              failed = true;
              break;
            case OPENEVSE_STATE_NO_EARTH_GROUND:
              state_msg = "接地檢查失敗";
              failed = true;
              break;
            case OPENEVSE_STATE_STUCK_RELAY:
              state_msg = "繼電器阻塞";
              failed = true;
              break;
            case OPENEVSE_STATE_GFI_SELF_TEST_FAILED:
              state_msg = "GFI自檢失敗";
              failed = true;
              break;
            case OPENEVSE_STATE_OVER_TEMPERATURE:
              state_msg = "溫度過高";
              failed = true;
              break;
            case OPENEVSE_STATE_OVER_CURRENT:
              state_msg = "電流過高";
              failed = true;
              break;
            case OPENEVSE_STATE_SLEEPING:
              state_msg = "睡眠中"; break;
            case OPENEVSE_STATE_DISABLED:
              state_msg = "設備已鎖定"; break;
            default:
              state_msg = "不明的狀態";
              failed = true;
              break;
          }
          if (failed) {
            rapiSender.sendCmd("$FD");
            scr.bootDrawFrame();
            scr.bootDrawError(state_msg);
            while (true)
              delay(10);
          } else {
            bool locked = true;
            if ( (vflags & OPENEVSE_VFLAG_AUTH_LOCKED) == OPENEVSE_VFLAG_AUTH_LOCKED) {
              locked = true;
            } else {
              locked = false;
            }

            scr.normalDrawPlugStatus(state_msg);
            
            if (locked) {
              scr.normalDrawDeviceStatus(true, 0);
            } else {
              struct tm timeinfo;
              time_t now;
              getLocalTime(&timeinfo);
              time(&now);

              if (epochValidUntil <= now) {
                rapiSender.sendCmd("$S4 1");
              } else {
                lock_countdown = epochValidUntil - now;
                scr.normalDrawDeviceStatus(false, lock_countdown);
              }
              
            }
          }
        }
      });
      OpenEVSE.getChargeCurrentAndVoltage([](int ret, double amps, double volts) {
        if (RAPI_RESPONSE_OK == ret) {
          if (lastVolts != volts) {
            scr.normalDrawConcurrentVoltage(volts);
          } 

          if (lastAmps != amps) {
            scr.normalDrawConcurrentAmp(amps);
          }
          lastVolts = volts;
          lastAmps = amps;
        }
      });
    }
    else
    {
      OpenEVSE.begin(rapiSender, [](bool connected)
      {
        if(connected)
        {
          log_e("Connected to OpenEVSE\n");
        } else {
          log_e("OpenEVSE not responding or not connected");
        }
      });
    }
  }
  scr.normalDrawDateTime();
  
  if ( !mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial() ) {
    delay(10);
    log_e("Nothing scanned");
    return;
  }

  log_e("Something scanned");
  String card_uuid = mfrc522.GetCardIdString();
  log_e("Card ID: %s", card_uuid);
  bool checked = false;
  for (int x=0; x<10; x++) {
    if (setting.validTag[x].length()==0) break;
    if (setting.validTag[x] == card_uuid) {
      checked = true;
    }
  }
  checked = true;

  if (checked) {
    buzzer.Success();
    rapiSender.sendCmd("$S4 0");
    epochValidUntil = getTime()+180;
  } else {
    buzzer.Fail();
    epochValidUntil=0;
  }

  vTaskDelay(1000);
}
