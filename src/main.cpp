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
#include <Adafruit_MCP9808.h>
// 自己管的部分
#include <Wiegand.h>
#include <buzzer.h>
#include <screen.h>
#include <define.h>
#include <utils.h>
#include <records.h>

// Controller Version
String controllerVersion = "1.0.0-20131205";
// Controller Version

RapiSender rapiSender(&RAPI_PORT);
Screen scr = Screen(LCD_CS, LCD_RST, LCD_SCK, LCD_MISO, LCD_MOSI);
Adafruit_MCP9808 tempSensor;
Setting setting;
String eth_ip, eth_mac;
WIEGAND wg;
File RFID_DATA;
Buzzer buzzer = Buzzer(BEEPER,PWMCHANNEL,RESOLUTION);
unsigned long lock_countdown=0;
uint32_t next_status = 0;

float lastVolts=-1;
double  lastAmps=-1, lastWalts=-1, maxAmps = -1;
double lastSessionWatts = -1, lastTotalWatts = -1;

unsigned long epochValidUntil=0;

bool isCharging = false, lastIsCharging = false;
bool isConnected = false, lastIsConnected = false;
bool isConnectEMS = false;
bool isRecordFull = true;
String lastAuthenCard = "";
String evseVersion = "", evseProtocol="";
bool isLocked = true;
bool isAuthByCard = false;
WebServer server(80);

LocalRecords locRec;
ChargeRecord lastRecord;
unsigned long lastConnectTime=0;
float RMSvoltage1 = 0;

void IRAM_ATTR EM() {
  /*
  rapiSender.sendCmd("$FD");
  log_e("SHUT DOWN EVSE!!!!");
  vTaskDelay(5000);
  ESP.restart();
  */
}

void apiHandleOn() {
  rapiSender.sendCmd("$FE");
  log_e("form EMS: 允許充電");
  isLocked = false;
  server.send(200, F("application/json"), F("{ \"eves_status\": \"On\" }"));
}

void apiHandleOff() {
  rapiSender.sendCmd("$FD");
  log_e("form EMS: 停止充電");
  isLocked = true;
  server.send(200, F("application/json"), F("{ \"eves_status\": \"Off\" }"));

}

void apiHandleInfo() {
  lastConnectTime = (millis()/1000);
  String result = "{";
  result = result + "\"amps\":" + String(lastAmps) + ",";
  result = result + "\"volts\":" + String(lastVolts) + ",";
  result = result + "\"maxapms\":" + String(maxAmps) + ",";
  result = result + "\"charging\":\"" + (lastIsCharging? "true":"false") + "\","; 
  result = result + "\"plugged\":\"" + (lastIsConnected ? "true": "false") + "\","; 
  result = result + "\"locked\":\"" + (isLocked ? "true": "false") + "\",";
  result = result + "\"card\":\"" + lastAuthenCard + "\",";
  result = result + "\"hostname\":\"" + setting.name + "\",";
  result = result + "\"ipaddr\":\"" + eth_ip + "\",";
  result = result + "\"mac\":\"" + eth_mac + "\",";
  result = result + "\"evseVersion\":\"" + evseVersion + "\",";
  result = result + "\"evseProtocol\":\"" + evseProtocol + "\",";
  result = result + "\"controllerVersion\":\"" + controllerVersion + "\"";
  result = result + "}";

  server.send(200, F("application/json"), result);
}

void apiAuth() {
  String cardUuid = server.pathArg(0);
  log_e("Form EMS : Authen by CardID: %s", cardUuid);
  rapiSender.sendCmd("$S4 0");
  server.send(200, F("application/json"), F("{ \"auth_status\": \"Unlocked\" }"));
  epochValidUntil = (millis()/1000) + 180;
}

void apiUnauth() {
  //String cardUuid = server.pathArg(0);
  //log_e("Authen by CardID: %s", cardUuid);
  log_e("Form EMS : card_auth is failed");
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
  log_e("From EMS : Turn Amp to %s", amps);
  String cmd = "$SC "+ amps + " V";
  rapiSender.sendCmd(cmd);
  server.send(200, F("application/json"), F("{ \"message\": \"done\" }"));
}


void apiHandleClearConfig() {
  SPIFFS.begin(true);
  if (SPIFFS.exists("/setting")) {
    SPIFFS.remove("/setting");
    log_e("Form EMS : Clear config");
  }
  SPIFFS.end();
  server.send(200, F("application/json"), F("{ \"message\": \"done\" }"));
  log_e("Delay 5 secs to reboot");
  vTaskDelay(5000);
  ESP.restart();
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
      cnt++;
    }
  }
  String result= "[";
  for (int x=0; x<cnt; x++) {
    result = result + tags[x];
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
  log_e("Form EMS : AddTag , CardID: %s", uuid);
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
  log_e("Form EMS : DelTag , CardID: %s", uuid);
  server.send(200, F("application/json"), F("{ \"message\": \"done\" }"));
}

void apiHandleSetName() {
  String name = server.pathArg(0);
  setting.name = name;
  writeConfig();
  log_e("Form EMS : SetName , Name: %s", name);
  server.send(200, F("application/json"), F("{ \"message\": \"done\" }"));
  vTaskDelay(5000);
  ESP.restart();
}

void apiHandleRecord() {
  if (isRecordFull == true){
    log_e("evse_tuneOn");
    rapiSender.sendCmd("$FE");
    isRecordFull = false;
  }
  log_e("[Record] Enabled by flush records");
  locRec.dumpRecords();
  //lastConnectTime = (millis()/1000);
  server.send(200, F("application/json"), locRec.getJsonRecords());
}

void setup()
{
  Serial.begin(115200);
  Serial1.begin(ATMEGA32_BAUD, SERIAL_8N1, ATMEGA32_RX, ATMEGA32_TX);
  while (!Serial && millis() < 5000);
    delay(500);
  scr.begin(RA8875_480x272);

  rapiSender.sendCmd("$FD");
  rapiSender.sendCmd("$S4 0");

  scr.bootDrawFrame();
  scr.bootDrawStatu("設定腳位");
  vTaskDelay(100);
  pinMode(LED1,OUTPUT);
  pinMode(LED2,OUTPUT);
  pinMode(LED3,OUTPUT);
  digitalWrite(LED1,HIGH);
  digitalWrite(LED2,HIGH);
  digitalWrite(LED3,HIGH);

  scr.bootDrawStatu("設定揚聲器");
  vTaskDelay(100);
  buzzer.begin();
  buzzer.Success();


  int count = 0;
  scr.bootDrawStatu("設定序列通訊");
  vTaskDelay(100);
  Wire.begin(RFID_SDA, RFID_SCL);
  log_e("[Setup] Starting I2C scan:");
  for (byte i = 8; i < 128; i++)
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
  vTaskDelay(100);

  scr.bootDrawStatu("設定溫度傳感器");
  vTaskDelay(100);
  tempSensor.begin();

  digitalWrite(LED1,LOW);
  digitalWrite(LED2,LOW);
  
  // 預先處理 mac
  uint64_t chipId = ESP.getEfuseMac();
  byte mac[] = {0xDE, 0xAD, 0xBE, 0, 0, 0};
  mac[5] = 0xFF & (chipId >> 24);
  mac[4] = 0xFF & (chipId >> 32);
  mac[3] = 0xFF & (chipId >> 40);
  char hostname[12];
  sprintf(hostname, "EVSE_%02x%02x%02x", mac[3], mac[4], mac[5]);

  scr.bootDrawStatu("設定檔案系統");
  vTaskDelay(100);
  log_e("[Setup] SPIFFS Init");
  if (!SPIFFS.begin(true)) {
    log_e("[Setup] SPIFFS failed");
    return;
  }
  log_e("[Setup] SPIFFS Inited");
  
  scr.bootDrawStatu("讀取設定值");
  vTaskDelay(100);
  if (!SPIFFS.exists("/setting")) {
    // First time;
    log_e("[Setup] Init setting files");
    char buf[5];
    setting.name = hostname;
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

  locRec.init();
  locRec.readFile();

  scr.bootDrawStatu("設定網路卡");
  vTaskDelay(100);

  log_e("[Setup] ESP32 WiFi Mac: %012llx", chipId);
  log_e("[Setup] W5500 Mac: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  log_e("[Setup] W5500 Init");
  
  ESP32_W5500_onEvent();
  log_e("[Setup] W5500 onEvent done");
  ETH.begin(ETH_MISO, ETH_MOSI, ETH_SCK, ETH_CS, ETH_INT, 25, 2, mac);
  
  log_e("[Setup] Hostname: %s", setting.name);
  ETH.setHostname((setting.name.c_str()));
  log_e("[Setup] W5500 begin done");

  scr.bootDrawStatu("取得IP....");
  vTaskDelay(100);
  ESP32_W5500_waitForConnect();
  eth_ip = IpAddress2String(ETH.localIP());
  eth_mac = ETH.macAddress();

  log_e("[Setup] W5500 Final Status");
  log_e("[Setup] \tIP Addr: %s", IpAddress2String(ETH.localIP()));
  log_e("[Setup] \tSubnet: %s", IpAddress2String(ETH.subnetMask()));
  log_e("[Setup] \tGateway: %s", IpAddress2String(ETH.gatewayIP()));
  log_e("[Setup] \tDNS: %s", IpAddress2String(ETH.dnsIP()));

  scr.bootDrawStatu("起始網路對時");
  //configTime(28800, 3600, "time.stdtime.gov.tw");
  vTaskDelay(100);

  scr.bootDrawStatu("起始服務綁定");
  vTaskDelay(100);

  server.on(F("/turnOff"), apiHandleOff);
  server.on(F("/turnOn"), apiHandleOn);
  server.on(F("/info"), apiHandleInfo);
  server.on(F("/record"), apiHandleRecord);

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
  vTaskDelay(100);
  pinMode(BUTTON_1, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(BUTTON_1), EM, RISING);
  
  scr.bootDrawDone();
  vTaskDelay(200);
  // Normal mode
  scr.normalDrawFrame(eth_mac, eth_ip, setting.name);
  scr.normalDrawDeviceStatus(isAuthByCard,isLocked);
  buzzer.Success();

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
            digitalWrite(LED3,HIGH);
          }
          else
          {
            digitalWrite(LED3,LOW);
          }
          String state_msg;
          bool failed = false;
          isCharging = false;
          switch (evse_state) {
            case OPENEVSE_STATE_STARTING:
              state_msg = "開機中...."; break;
            case OPENEVSE_STATE_NOT_CONNECTED:
              state_msg = "車輛未連接"; 
              isConnected = false;
              break;
            case OPENEVSE_STATE_CONNECTED:
              state_msg = "車輛已連接"; 
              isConnected = true;
              break;
            case OPENEVSE_STATE_CHARGING:
              state_msg = "車輛充電中"; 
              isCharging = true;
              break;
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
          // if (failed) {
          //   log_e("發生錯誤");
          //   rapiSender.sendCmd("$FD");
          //   scr.bootDrawFrame();
          //   scr.bootDrawError(state_msg);
            // while (true)
            //   delay(10);
          //} else {
            
            // if ( (vflags & OPENEVSE_VFLAG_AUTH_LOCKED) == OPENEVSE_VFLAG_AUTH_LOCKED) {
            //   isLocked = true;
            // } else {
            //   isLocked = false;
            // }

            scr.normalDrawPlugStatus(state_msg);
            //scr.normalDrawDeviceStatus(isLocked);
            // if (isLocked) {
            //   scr.normalDrawDeviceStatus(true);
            // } else {

              // struct tm timeinfo;
              // time_t now;
              // getLocalTime(&timeinfo);
              // time(&now);
              /*
              if (epochValidUntil <= now) {
                rapiSender.sendCmd("$S4 1");
                lastAuthenCard = "";
              } else {
                lock_countdown = epochValidUntil - now;
                scr.normalDrawDeviceStatus(false, lock_countdown);
              }
              */
            //}
          //}
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
      OpenEVSE.getEnergy([](int ret, double session_wh, double total_kwh) {
        if (RAPI_RESPONSE_OK == ret) {
          if (lastSessionWatts != session_wh) {
            scr.normalDrawConcurrentWatts(session_wh);
          }

          /*
          if (lastTotalWatts != total_kwh) {
            scr.normalDrawTotalWatts(total_kwh);
          }
          */

          lastSessionWatts = session_wh;
          lastTotalWatts = total_kwh;
        }
      }); 
      if (evseVersion == "") {
        OpenEVSE.getVersion([](int ret, const char* firmware, const char* protocol) {
          if (RAPI_RESPONSE_OK == ret) {
            evseVersion = String(firmware);
            evseProtocol = String(protocol);
          }
        });
      }
       /* 充電電流判定 */
      OpenEVSE.getCurrentCapacity([](int ret, long min_current, long pilot, long max_configured_current, long max_hardware_current){
        if (RAPI_RESPONSE_OK == ret) {
          
            maxAmps = max_configured_current;
            scr.normalDrawTotalWatts(maxAmps);
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

  // 溫度
  sensors_event_t event;
  tempSensor.getEvent(&event);
  scr.normalDrawTemp(event.temperature);
  scr.normalDrawDeviceStatus(isAuthByCard,isLocked);
  // if (voltageSensor1.getRmsVoltage()<150)
  // {
  //   voltageSensor1.setSensitivity(576);
  //   lastVolts = voltageSensor1.getRmsVoltage();
  // }
  // else
  // {
  //   voltageSensor1.setSensitivity(656);
  //   lastVolts = voltageSensor1.getRmsVoltage();
  // }
  // scr.normalDrawConcurrentVoltage(lastVolts);
  // scr.normalDrawDateTime();


  if (lastIsCharging == true && isCharging == false) {
    // 從充電 -> 不充電 ==> 充電結束鎖定
    log_e("From CHARGING to NOTCHARGING");
    lastRecord.chargeStopAt = (millis()/1000);
    lastRecord.usedWatt = lastSessionWatts;
    locRec.append(lastRecord);
    
    locRec.dumpRecords();

    if (locRec.isFull()) {
      // 滿五次
      isRecordFull = true;
      rapiSender.sendCmd("$FD");
      log_e("[Record] Disable on records full");
    }
  } else if (lastIsCharging == false && isCharging == true) {
    // 從非充電 -> 充電 ==> 充電起始
    log_e("From NOTCHARGING to CHARGING");
    lastRecord.chargeStartAt = (millis()/1000);
    lastRecord.cardId = lastAuthenCard;
  } else {
    //log_e("CHARGING Status= %s",isCharging? "true": "false" );
  }
  lastIsCharging = isCharging;

  //連接到電動車
  if (lastIsConnected == true && isConnected == false) {
    log_e("Plug From CONNECTED to DISCONNECTED");
    isAuthByCard = false;
    scr.normalDrawDeviceStatus(isAuthByCard,!isLocked);
    lastAuthenCard = "";
    //rapiSender.sendCmd("$S4 0");
  } else if (lastIsConnected == false && isConnected == true) {
    log_e("Plug From DISCONNECT to CONNECTED");
  } else {
    //log_e("Plug Status= %s", isConnected? "true": "false" );
  }
  lastIsConnected = isConnected;

  // 斷線後無論如何都是 8A
  if (((millis()/1000) - lastConnectTime > DISCONNECT_INTERVAL) && isConnectEMS) {
    rapiSender.sendCmd("$SC 8 V");
    log_e("[WARNING] Drop load to 8A by DISCONNECT detection");
    isConnectEMS = false;
    digitalWrite(LED2,LOW);
  }
  else if(((millis()/1000) - lastConnectTime) < DISCONNECT_INTERVAL)
  {
    isConnectEMS = true;
    digitalWrite(LED2,HIGH);
  }

  if ( !wg.available() || !isConnected || isLocked) {
    delay(50);
    return;
  }

  String card_uuid = String(wg.getCode(),HEX);
  log_e("Card ID: %s", card_uuid);
  bool checked = false;
  for (int x=0; x<10; x++) {
    if (setting.validTag[x].length()==0) break;
    if (setting.validTag[x] == card_uuid) {
      checked = true;
    }
  }
  //checked = true;
  if (checked) {
    log_e("Card ID: %s is valid", card_uuid);
    buzzer.Success();
    // 跟上一張一樣卡號的話, 鎖上
    if (card_uuid == lastAuthenCard) {
      log_e("From UNLOCK to LOCK");
      isAuthByCard = false;
      scr.normalDrawDeviceStatus(isAuthByCard,isLocked);
      rapiSender.sendCmd("$S4 1"); 
      lastAuthenCard = "";
      //epochValidUntil=0;
    } else {
      log_e("From LOCK to UNLOCK");
      isAuthByCard = true;
      scr.normalDrawDeviceStatus(isAuthByCard,isLocked);
      rapiSender.sendCmd("$S4 0");
      //epochValidUntil = (millis()/1000)+180;
      lastAuthenCard = card_uuid;
    }
  } else {
    log_e("Card ID: %s is invalid", card_uuid);
    buzzer.Fail();
    epochValidUntil=0;
  }

  vTaskDelay(100);
}
