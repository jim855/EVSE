#include "buzzer.h"

Buzzer::Buzzer(int buzzerPin, int pwmChannel, int RESOLUTION)
{
    pin = buzzerPin;
    channel = pwmChannel;
    res = RESOLUTION;
    pinMode(pin, OUTPUT);
}

void Buzzer::begin()
{
    ledcSetup(channel, f1, res);
    ledcAttachPin(pin, channel);
    ledcWrite(channel, DUTYCLE);
    delay(50);
    ledcWrite(channel, 0);
}

void Buzzer::Success()
{
    ledcSetup(channel, f1, res);
    ledcWrite(channel, DUTYCLE);
    delay(50);
    ledcWrite(channel, 0);
}

void Buzzer::Fail()
{
    ledcSetup(channel, f2, res);
    ledcWrite(channel, DUTYCLE);
    delay(400);
    ledcWrite(channel, 0);
}

void Buzzer::launch(int fre)
{
    ledcSetup(channel, fre, res);
    ledcWrite(channel, DUTYCLE);
    delay(50);
    ledcWrite(channel, 0);
}

void Buzzer::setSuccessFRE(int newF1)
{
    f1 = newF1;
}

void Buzzer::setFailFRE(int newF2)
{
    f2 = newF2;
}