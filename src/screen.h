#include <Adafruit_GFX.h>
#include <Adafruit_RA8875.h>
#include <U8g2_for_Adafruit_GFX.h>

class Screen {
    public:
        Screen(uint8_t cs, uint8_t rst, uint8_t sck, uint8_t miso, uint8_t mosi);
        bool begin(RA8875sizes s);
        void bootDrawFrame();
        void bootDrawStatu(String status);
        void bootDrawError(String error);
        void bootDrawDone();

        void normalDrawFrame(String mac, String ip, String position);
        void normalDrawConcurrentVoltage(double voltage);
        void normalDrawConcurrentAmp(double amp);
        void normalDrawConcurrentWatts(double watts);
        void normalDrawTotalWatts(double watts);
        void normalDrawTemp(double temp);

        void normalDrawPlugStatus(String status);
        void normalDrawDateTime(String date, String time);
        
        void normalDrawDateTime();
        void normalDrawDeviceStatus(bool locked, unsigned long count);
        void normalDrawDeviceWait();
        void normalDrawDeviceMessage(String message, bool isError);
        
    private:
        Adafruit_RA8875 _tft;
        U8G2_FOR_ADAFRUIT_GFX _u8g2;
        uint8_t _p_cs, _p_rst, _p_sck, _p_miso, _p_mosi;
        RA8875sizes _size;
        void twolayerMode();
        void Source_layer1(void);
        void Source_layer2(void);
        void Destination_layer1(void);
        void Destination_layer2(void);
        void BTE_Source_Destination(uint16_t XL,uint16_t XR ,uint16_t YT ,uint16_t YB);
        void BTE_Size_setting(uint16_t X,uint16_t Y);
        void BTE_ROP_Code(uint8_t setx);
        void Chk_Busy_BTE(void);
        void memorytolayer(int layer);
        void displaytolayer(int layer);
        void btememorycopy(uint8_t source_layer,uint16_t s_x,uint16_t s_y,
                                      uint8_t destination_layer,uint16_t des_x,uint16_t des_y,
                                      uint16_t block_width,uint16_t block_height);
};
