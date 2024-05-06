#include <Adafruit_GFX.h>
#include <Adafruit_RA8875.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "screen.h"
#include "fonts.h"

Screen::Screen(uint8_t cs, uint8_t rst, uint8_t sck, uint8_t miso, uint8_t mosi) : _tft(cs, rst, sck, miso, mosi)
{
    _p_cs = cs;
    _p_rst = rst;
    _p_sck = sck;
    _p_miso = miso;
    _p_mosi = mosi;
    _tft = Adafruit_RA8875(_p_cs, _p_rst, _p_sck, _p_miso, _p_mosi);
}

bool Screen::begin(RA8875sizes s)
{
    _size = s;

    _u8g2.begin(_tft);
    if (!_tft.begin(_size))
    {
        log_e("[Display::begin] RA8875 begin failed");
        return false;
    }
    _u8g2.begin(_tft);

    _tft.displayOn(true);
    _tft.GPIOX(true);
    _tft.PWM1config(true, RA8875_PWM_CLK_DIV1024);
    _tft.PWM1out(255);
    _tft.fillScreen(RA8875_BLACK);
    _tft.textMode();
    return true;
}
void Screen::twolayerMode()
{
    _tft.writeCommand(0x20);
    _tft.writeData(0x80);
}

void Screen::memorytolayer(int layer)
{
    if(layer == 1){
    _tft.writeCommand(0x41);
    // Set bit 1
    _tft.writeData(0x00);
  }
  else{
    _tft.writeCommand(0x41);
    // Set bit 1
    _tft.writeData(0x01);
  }
  delay(17);
}

void Screen::displaytolayer(int layer)
{
    if(layer == 1){
    _tft.writeCommand(0x52);
    // Set bit 1
    _tft.writeData(0x00);
  }
  else  {
    _tft.writeCommand(0x52);
    // Set bit 1
    _tft.writeData(0x01);
  }
  delay(17);
}

void Screen::Source_layer1(void)
{
    uint8_t temp;
    _tft.writeCommand(0x57);//VSBE1
     temp = _tft.readData();
     temp &= 0x80 ;
    _tft.writeData(temp);
}

void Screen::Source_layer2(void)
{
    uint8_t temp;
    _tft.writeCommand(0x57);//VSBE1
    temp = _tft.readData();
    temp |= 0x80 ;
    _tft.writeData(temp);
}

void Screen::Destination_layer1(void)
{
    uint8_t temp;
    _tft.writeCommand(0x5B);//VSBE1
     temp = _tft.readData();
     temp &= 0x80 ;
    _tft.writeData(temp);
}

void Screen::Destination_layer2(void)
{
    uint8_t temp;
    _tft.writeCommand(0x5B);//VSBE1
    temp = _tft.readData();
    temp |= 0x80 ;
    _tft.writeData(temp);
}

void Screen::BTE_Source_Destination(uint16_t XL,uint16_t XR ,uint16_t YT ,uint16_t YB)
{
    uint8_t temp,temp1;
    
    temp=XL;   
    _tft.writeCommand(0x54);//HSBE0
    _tft.writeData(temp);
    temp=XL>>8;   
    _tft.writeCommand(0x55);//HSBE1     
    _tft.writeData(temp);

    temp=XR;   
    _tft.writeCommand(0x58);//HDBE0
    _tft.writeData(temp);
    temp=XR>>8;   
    _tft.writeCommand(0x59);//HDBE1     
    _tft.writeData(temp); 
    
    temp=YT;   
    _tft.writeCommand(0x56);//VSBE0
    _tft.writeData(temp);
    temp=YT>>8;   
    _tft.writeCommand(0x57);//VSBE1
    temp1 = _tft.readData();
    temp1 &= 0x80;
    temp=temp|temp1; 
    _tft.writeCommand(0x57);//VSBE1  
    _tft.writeData(temp);


    temp=YB;   
    _tft.writeCommand(0x5a);//VDBE0
    _tft.writeData(temp);
    temp=YB>>8;   
    _tft.writeCommand(0x5b);//VDBE1
    temp1 = _tft.readData();
    temp1 &= 0x80;
    temp=temp|temp1;  
    _tft.writeCommand(0x5b);//VDBE1   
    _tft.writeData(temp);
}

void Screen:: BTE_Size_setting(uint16_t X,uint16_t Y)
{  
    _tft.writeCommand(0x5c);//BEWR0
  _tft.writeData(X); 
    _tft.writeCommand(0x5d);//BEWR1     
  _tft.writeData(X>>8);
  
    _tft.writeCommand(0x5e);//BEHR0
  _tft.writeData(Y);   
    _tft.writeCommand(0x5f);//BEHR1     
  _tft.writeData(Y>>8);
}                  


void Screen::BTE_ROP_Code(uint8_t setx)
{  
    _tft.writeCommand(0x51);//BECR1     
    _tft.writeData(setx); 
}                    

void Screen:: Chk_Busy_BTE(void)
{
  uint8_t temp;
    
  do
  {
     temp=_tft.readStatus();
  }while((temp&0x40)==0x40);     
}


void Screen::btememorycopy(uint8_t source_layer,uint16_t s_x,uint16_t s_y,
                                      uint8_t destination_layer,uint16_t des_x,uint16_t des_y,
                                      uint16_t block_width,uint16_t block_height)
{
  if(source_layer==1)
  Source_layer1();
  else if (source_layer==2)
  Source_layer2();

  if(destination_layer==1)
  Destination_layer1();
  else if (destination_layer==2)
  Destination_layer2();

  BTE_Source_Destination(s_x,des_x,s_y,des_y);
  BTE_Size_setting(block_width,block_height); 
  BTE_ROP_Code(0xc2); //(Move BTE in positive direction with ROP.)
   
  _tft.writeCommand(0x50); //BTE enable
  _tft.writeData(0x80);
  Chk_Busy_BTE();

}

void Screen::bootDrawFrame()
{
    // 開機中框架畫面
    memorytolayer(2);
    _tft.fillScreen(RA8875_BLACK);
    _tft.fillRect(0, 0, 480, 48, RA8875_GREEN);
    _u8g2.setFontMode(2);
    _u8g2.setFontDirection(0);
    _u8g2.setForegroundColor(RA8875_BLUE);
    _u8g2.setBackgroundColor(RA8875_GREEN);
    _u8g2.setFont(cht_font_32);
    _u8g2.setCursor(0, 40);
    _u8g2.print("EVSE智慧型充電樁");

    _u8g2.setForegroundColor(RA8875_WHITE);
    _u8g2.setBackgroundColor(RA8875_BLACK);
    _u8g2.setFont(cht_font_32);
    _u8g2.setCursor(100, 120);
    _u8g2.print("開機中....");
    btememorycopy(2,0,0,1,0,0,480,272);
}

void Screen::bootDrawStatu(String status)
{
    memorytolayer(2);
    _tft.fillRect(100, 120, 480, 160, RA8875_BLACK);
    _u8g2.setForegroundColor(RA8875_WHITE);
    _u8g2.setBackgroundColor(RA8875_BLACK);
    _u8g2.setCursor(100, 160);
    _u8g2.setFont(cht_font_24);
    _u8g2.print("目前狀態 => " + status);
    btememorycopy(2,0,0,1,0,0,480,272);
}

void Screen::bootDrawError(String error)
{
    memorytolayer(2);
    _tft.fillRect(100, 120, 480, 160, RA8875_BLACK);
    _u8g2.setForegroundColor(RA8875_RED);
    _u8g2.setBackgroundColor(RA8875_BLACK);
    _u8g2.setCursor(100, 160);
    _u8g2.setFont(cht_font_24);
    _u8g2.print("開機失敗 => " + error);
    _u8g2.setCursor(100, 200);
    _u8g2.setFont(cht_font_24);
    _u8g2.print("請嘗試重新開機，或聯繫管理員");
    btememorycopy(2,0,0,1,0,0,480,272);
}

void Screen::bootDrawDone()
{
    memorytolayer(2);
    _tft.fillRect(100, 120, 480, 272, RA8875_BLACK);
    _u8g2.setForegroundColor(RA8875_WHITE);
    _u8g2.setBackgroundColor(RA8875_BLACK);
    _u8g2.setFont(cht_font_32);
    _u8g2.setCursor(100, 120);
    _u8g2.print("開機完成，進入正常模式");
    _u8g2.setCursor(100, 160);
    _u8g2.print("請稍後....");
    btememorycopy(2,0,0,1,0,0,480,272);
}

void Screen::normalDrawFrame(String mac, String ip, String position)
{
    memorytolayer(2);
    // Color Block
    _tft.fillRect(0, 48, 240, 240, RA8875_CYAN);
    _tft.fillRect(240, 48, 240, 240, RA8875_BLACK);
    _tft.fillRect(0, 240, 480, 32, RA8875_GREEN);
    // IP & Mac
    _u8g2.setForegroundColor(RA8875_BLUE);
    _u8g2.setBackgroundColor(RA8875_GREEN);
    _u8g2.setFont(cht_font_16);
    _u8g2.setCursor(330, 20);
    _u8g2.print(mac);
    _u8g2.setForegroundColor(RA8875_BLUE);
    _u8g2.setBackgroundColor(RA8875_GREEN);
    _u8g2.setFont(cht_font_16);
    _u8g2.setCursor(330, 40);
    _u8g2.print(ip);
    // Position
    _u8g2.setForegroundColor(RA8875_BLACK);
    _u8g2.setBackgroundColor(RA8875_GREEN);
    _u8g2.setFont(cht_font_24);
    _u8g2.setCursor(250, 264);
    _u8g2.print("車位：" + position);
    btememorycopy(2,0,0,1,0,0,480,272);
}
void Screen::normalDrawConcurrentVoltage(double voltage)
{
    memorytolayer(2);
    _tft.fillRect(310, 48, 170, 36, RA8875_BLACK);
    _u8g2.setForegroundColor(RA8875_WHITE);
    _u8g2.setBackgroundColor(RA8875_BLACK);
    _u8g2.setFont(cht_font_24);
    _u8g2.setCursor(250, 84);
    _u8g2.print("電壓: " + String(voltage) + " V");
    btememorycopy(2,0,0,1,0,0,480,272);
}
void Screen::normalDrawConcurrentAmp(double amp)
{
    memorytolayer(2);
    _tft.fillRect(310, 84, 170, 36, RA8875_BLACK);
    _u8g2.setForegroundColor(RA8875_WHITE);
    _u8g2.setBackgroundColor(RA8875_BLACK);
    _u8g2.setFont(cht_font_24);
    _u8g2.setCursor(250, 120);
    _u8g2.print("電流: " + String(amp) + " A");
    btememorycopy(2,0,0,1,0,0,480,272);
}
void Screen::normalDrawConcurrentWalts(double walts)
{
    memorytolayer(2);
    _tft.fillRect(310, 120, 170, 36, RA8875_BLACK);
    _u8g2.setForegroundColor(RA8875_WHITE);
    _u8g2.setBackgroundColor(RA8875_BLACK);
    _u8g2.setFont(cht_font_24);
    _u8g2.setCursor(250, 156);
    _u8g2.print("度數: " + String(walts) + " kWh");
    btememorycopy(2,0,0,1,0,0,480,272);
}
void Screen::normalDrawPlugStatus(String status)
{
    memorytolayer(2);
    // Position
    _tft.fillRect(0, 240, 240, 272, RA8875_GREEN);
    _u8g2.setForegroundColor(RA8875_BLACK);
    _u8g2.setBackgroundColor(RA8875_GREEN);
    _u8g2.setFont(cht_font_16);
    _u8g2.setCursor(10, 264);
    _u8g2.print("狀態: " + status);
    btememorycopy(2,0,0,1,0,0,480,272);
}

void Screen::normalDrawDateTime()
{
    memorytolayer(2);
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        return;
    }
    char buf[20];
    _tft.fillRect(310, 156, 170, 72, RA8875_BLACK);
    _u8g2.setForegroundColor(RA8875_WHITE);
    _u8g2.setBackgroundColor(RA8875_BLACK);
    _u8g2.setFont(cht_font_24);

    strftime(buf, 20, "%Y/%m/%d", &timeinfo);
    _u8g2.setCursor(250, 188);
    _u8g2.print(String(buf));

    strftime(buf, 20, "%H:%M:%S", &timeinfo);
    _u8g2.setCursor(250, 212);
    _u8g2.print(String(buf));
    btememorycopy(2,0,0,1,0,0,480,272);
}

void Screen::normalDrawDeviceStatus(bool locked, unsigned long count) {
    memorytolayer(2);
    if (locked) {
        _tft.fillRect(0, 48, 240, 64, RA8875_CYAN);
        _u8g2.setForegroundColor(RA8875_RED);
        _u8g2.setBackgroundColor(RA8875_CYAN);
        _u8g2.setFont(cht_font_32);
        _u8g2.setCursor(10, 92);
        _u8g2.print("設備鎖定中");
        _tft.fillRect(0, 96, 240, 48, RA8875_CYAN);
        _u8g2.setForegroundColor(RA8875_BLACK);
        _u8g2.setBackgroundColor(RA8875_CYAN);
        _u8g2.setFont(cht_font_24);
        _u8g2.setCursor(10, 132);
        _u8g2.print("請刷卡解鎖");
    } 
    else 
    {
        _tft.fillRect(0, 48, 240, 64, RA8875_CYAN);
        _u8g2.setForegroundColor(RA8875_BLACK);
        _u8g2.setBackgroundColor(RA8875_CYAN);
        _u8g2.setFont(cht_font_32);
        _u8g2.setCursor(10, 92);
        _u8g2.print("設備開放中");
        _tft.fillRect(0, 96, 240, 48, RA8875_CYAN);
        _u8g2.setForegroundColor(RA8875_BLACK);
        _u8g2.setBackgroundColor(RA8875_CYAN);
        _u8g2.setFont(cht_font_24);
        _u8g2.setCursor(10, 132);
        _u8g2.print("鎖定倒數: " + String(count) + " 秒");
    }
    btememorycopy(2,0,0,1,0,0,480,272);
}


