#include <Arduino.h>

class Buzzer
{
public:
    Buzzer(int BEEPER, int PWMCHANNEL, int RESOLUTION);
    void begin();
    void Fail();
    void Success();
    void setSuccessFRE(int newF1);
    void setFailFRE(int newF2);
    void launch(int fre);

private:
    int pin;
    int channel;
    int res;
    int f1 = 3000;
    int f2 = 200;
    int DUTYCLE = 128;
};
