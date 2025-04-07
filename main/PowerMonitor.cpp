#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "hal/adc_types.h"
#include "smart_utility.h"
#include "PowerMonitor.h"
#include "SoundPlayer.h"
#include "SegDisplay.h"
#include "LedStrip.h"
#include "ASRCOmmandProc.h"
#include "AutoTurret.h"


extern "C" int adc_read_voltage(int channel_number);
const static char *TAG = "BATTERY_VOLTAGE";
static PowerMonitor g_powerMonitor;

struct capacity {
	int capacity;
	int min;
	int max;	
	int offset;
	int hysteresis;
};

int adc_offset = 30; //adc voltage offset, unit mv
#define BATTERY_LI_CAP_TABLE_ROWS   12
static struct capacity battery_capacity_tables_li[]= {
   /*percentage, min, max, hysteresis*/
	{0, 0, 3426, 0, 10},
	{1, 3427, 3638, 0, 10},
	{10,3639, 3697, 0, 10},
	{20,3698, 3729, 0, 10},
	{30,3730, 3748, 0, 10},
	{40,3749, 3776, 0, 10},
	{50,3777, 3827, 0, 10},
	{60,3828, 3895, 0, 10},
	{70,3896, 3954, 0, 10},
	{80,3955, 4050, 0, 10},
	{90,4051, 4119, 0, 10},
	{100,4120,4240, 0, 10},
};

static int find_item_by_voltage(int voltage)
{
    for(int i=0; i<11; i++)
    {
        if(voltage >= battery_capacity_tables_li[i].min 
            && voltage <= battery_capacity_tables_li[i].max) return battery_capacity_tables_li[i].capacity;
    }

    return -1;
}

int read_battery_voltage()
{
    int voltage = 0;
    voltage = adc_read_voltage(ADC1_CHAN_BATTERY);
    if(voltage < 0) {
        ESP_LOGE(TAG, "Failed to read battery voltage from ADC: %d.", voltage);
        return ESP_FAIL;
    }

    voltage = (voltage - adc_offset) * 2; //R = 1M + 1M

    ESP_LOGW(TAG, "Battery voltage %dmv.", voltage);
    return voltage;
}

int PowerMonitor::getPowerPercentage()
{
    int vt = read_battery_voltage();
    if(vt<0) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Battery voltage: %dmv.", vt);

    int cp = find_item_by_voltage(vt);
    if(cp < 0) {
        ESP_LOGE(TAG, "Failed to map battery voltage(%d) to capacity.", vt);
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "Battery capacity %d%%.", cp);
    return cp;
}

void PowerMonitor::vTimerCallback(void *ctx)
{
    PowerMonitor *handle = ( PowerMonitor* ) ctx;
    uint8_t nLowPowerCount = 0;
    while(1) {
        handle->m_nPowerPercentage = handle->getPowerPercentage();

        if(handle->m_nPowerPercentage<=30 && handle->m_nTimerRunCount%5==0) {       //warning every 5min
            SoundPlayer *sndPlayer = SoundPlayer::getInstance();
            sndPlayer->playSpeachWithNum("EB-PWRNO", handle->m_nPowerPercentage);
        }
        if(handle->m_nPowerPercentage<=10) {        //power is too low. sleep immediately
            if(nLowPowerCount>=3) {
                handle->tryDeepSleep(true);
            }else{
                nLowPowerCount++;
            }
        }else{
            nLowPowerCount = 0;
        }
        handle->m_nTimerRunCount++;

        handle->tryDeepSleep(false);
        vTaskDelay(pdMS_TO_TICKS(60*1000));
    }
}

int PowerMonitor::start()
{
    updateInputActivityTime();

    gpio_set_direction(GPIO_5VPWR_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_5VPWR_EN, 0);
    gpio_set_direction(GPIO_WEAPONPWR_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_WEAPONPWR_EN, 0);
    gpio_set_direction(GPIO_SERVO_TGT_PWR_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_SERVO_TGT_PWR_EN, 0);
    //gpio_set_direction(GPIO_SPK_MUTE, GPIO_MODE_OUTPUT);  //this has already been initialized by SoundPlayer
    //gpio_set_level(GPIO_SPK_MUTE, 0);

    xTaskCreate(PowerMonitor::vTimerCallback, "power_monitor_timer", 1024+1024+512, (void*)this, 1, NULL);
    //m_hTimer = xTimerCreate("power_monitor_timer",  pdMS_TO_TICKS( 60*1000 ), pdTRUE, this, PowerMonitor::vTimerCallback);
    //if(m_hTimer==NULL) return ESP_FAIL;

    //xTimerStart(m_hTimer, pdMS_TO_TICKS( 10*1000 ));
    return ESP_OK;
}

int PowerMonitor::stop()
{
    //BaseType_t rc = xTimerStop(m_hTimer, pdMS_TO_TICKS(1000));
    //if(rc==pdFAIL) return ESP_FAIL;
    return ESP_OK;
}

PowerMonitor* PowerMonitor::getInstance()
{
    return &g_powerMonitor;
}


void PowerMonitor::powerOff5V()
{
    gpio_set_level(GPIO_5VPWR_EN, 0);
}

void PowerMonitor::powerOffWeapon()
{
    gpio_set_level(GPIO_WEAPONPWR_EN, 0);
}

void PowerMonitor::powerOffTgtServo()
{
    gpio_set_level(GPIO_SERVO_TGT_PWR_EN, 0);
}

void PowerMonitor::powerOffSpeaker()
{
    gpio_set_level(GPIO_SPK_MUTE, 0);
}

void PowerMonitor::powerOn5V()
{
    gpio_set_level(GPIO_5VPWR_EN, 1);
}

void PowerMonitor::powerOnWeapon()
{
    gpio_set_level(GPIO_WEAPONPWR_EN, 1);
}

void PowerMonitor::powerOnTgtServo()
{
    gpio_set_level(GPIO_SERVO_TGT_PWR_EN, 1);
}

void PowerMonitor::powerOnSpeaker()
{
    gpio_set_level(GPIO_SPK_MUTE, 1);
}


void PowerMonitor::tryDeepSleep(bool bForceSleep)
{
    int64_t n = esp_timer_get_time() - m_nLastInputActivityTime;
    if(bForceSleep==false && n < 5 * 60 * 1000 * 1000) return;

    ESP_LOGW(TAG, "Entering deep sleep logic...");
    SoundPlayer *sndPlayer = SoundPlayer::getInstance();
    if(bForceSleep==true) sndPlayer->playSpeach("EB-PWRDN");
    else sndPlayer->playSpeach("EB-GOSLP");

    AutoTurret::getInstance()->stop();
    ASRCOmmandProc::sendCmd(CMD_TYPE_CTL, CTL_CMD_ASR_SLEEP);  //make ASR MCU sleep (not able to power if off)
    SegDisplay::getInstance()->clear();                        //turn off segment display
    LEDStrip::getInstance()->clear();                          //turn off LED Strip
    powerOffTgtServo();                                        //turn off target servo
    powerOff5V();                                              //turn off 5V boost IC (not able to really turn it off because of the limitation of MT36291)
    powerOffWeapon();                                          //turn off power for weapon devices (not able to really turn it off because of the limitation of MT36291)
    vTaskDelay(pdMS_TO_TICKS(10*1000));

    //rtc_gpio_isolate(GPIO_I2C_SDA);     //because it is pull up externally
    //rtc_gpio_isolate(GPIO_I2C_SCL);     //because it is pull up externally
    gpio_deep_sleep_hold_en();
    esp_deep_sleep_start();

}

void PowerMonitor::updateInputActivityTime()
{
    m_nLastInputActivityTime = esp_timer_get_time();
}