#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define ADC1_CHAN_BATTERY           ADC_CHANNEL_0   //GPIO_NUM_1
#define GPIO_SPK_MUTE               GPIO_NUM_15
#define GPIO_5VPWR_EN               GPIO_NUM_16
#define GPIO_WEAPONPWR_EN           GPIO_NUM_17
#define GPIO_I2C_SDA                GPIO_NUM_42
#define GPIO_I2C_SCL                GPIO_NUM_41
#define GPIO_SERVO_TGT_PWR_EN       GPIO_NUM_37


class PowerMonitor{
private:
    TimerHandle_t m_hTimer;
    int m_nPowerPercentage;
    uint32_t m_nTimerRunCount;
    int64_t m_nLastInputActivityTime;

    static void vTimerCallback( void *ctx );
    int getPowerPercentage();
    void tryDeepSleep(bool bForceSleep=false);
public:
    int start();
    int stop();
    int getPower() { return m_nPowerPercentage; };
    void updateInputActivityTime();
    int64_t getLastInputActivityTime() {return m_nLastInputActivityTime; };
public:
    static PowerMonitor* getInstance();
    static void powerOff5V();
    static void powerOffWeapon();
    static void powerOffTgtServo();
    static void powerOffSpeaker();
    static void powerOn5V();
    static void powerOnWeapon();
    static void powerOnTgtServo();
    static void powerOnSpeaker();
};

