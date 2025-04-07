#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "AutoTurret.h"
#include "IRMTransceiver.h"
#include "RadarMgr.h"
#include "SoundPlayer.h"
#include "PowerMonitor.h"
#include "LaserReceiver_IS0803.h"


#define GPIO_TURRET_SERVO_H  GPIO_NUM_18
#define GPIO_TURRET_SERVO_V  GPIO_NUM_8
#define GPIO_TURRET_ALERT_LED  GPIO_NUM_47
#define AUTOTURRET_STACK_SIZE   1024*3

#define LEDC_TIMER              LEDC_TIMER_2
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL_H          LEDC_CHANNEL_3
#define LEDC_CHANNEL_V          LEDC_CHANNEL_4
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT // Set duty resolution to 13 bits
//#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (50) // Frequency in Hertz. Set frequency at 50 Hz

static const char *TAG = "AUTO_TURRET";
static AutoTurret g_autoTurret;

static int calculatePWM(int degree)
{ //0-180度
 //20ms周期，高电平0.5-2.5ms，对应0-180度角度
  const float deadZone = 25.6;//对应0.5ms（0.5ms/(20ms/1024）) 舵机转动角度与占空比的关系：(角度/90+0.5)*1023/20
  const float max = 128;//对应2.5ms
  if (degree < 0)
    degree = 0;
  if (degree > 180)
    degree = 180;
  return (int)(((max - deadZone) / 180) * degree + deadZone);

}

AutoTurret* AutoTurret::getInstance()
{
    return &g_autoTurret;
}

int AutoTurret::init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 50Hz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel_H = {
        .gpio_num       = GPIO_TURRET_SERVO_H,
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_H,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_H));

#if 1
    ledc_channel_config_t ledc_channel_V = {
        .gpio_num       = GPIO_TURRET_SERVO_V,
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_V,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_V));
#endif 

    m_irmTransceiver.init();
    gpio_set_direction(GPIO_TURRET_ALERT_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_TURRET_ALERT_LED, 0);
    m_bStopFlag = false;
    m_bIsRunningState = false;
    return 0;
}

int AutoTurret::setAngleV(int degree)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_V, calculatePWM(degree)));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_V));
    return 0;
}


int AutoTurret::setAngleH(int degree)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_H, calculatePWM(degree)));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_H));
    return 0;
}


int AutoTurret::doPatrolH()
{
    int n = 3;
    while(--n>0 && m_bStopFlag==false) {
        setAngleH(m_degreeH);
        vTaskDelay(100 / portTICK_PERIOD_MS);

        if(m_degreeH<=30) {
            m_angleIncreaseH = true;
        }else if(m_degreeH>=150) {
            m_angleIncreaseH = false;
        }
        if(m_angleIncreaseH==true) {
            m_degreeH += 10;
        }else{
            m_degreeH -= 10;
        }
    }
    return 0;
}


int AutoTurret::doPatrolV()
{
    //return 0;  //电机轴被胶水粘住了，无法转动了

    int n = 2;
    while(--n>0 && m_bStopFlag == false) {
        setAngleV(m_degreeV);
        vTaskDelay(200 / portTICK_PERIOD_MS);

        if(m_degreeV<=86) {
            m_angleIncreaseV = true;
        }else if(m_degreeV>=96) {
            m_angleIncreaseV = false;
        }
        if(m_angleIncreaseV==true) {
            m_degreeV += 2;
        }else{
            m_degreeV -= 2;
        }
    }
    return 0;
}


int AutoTurret::doPatrol()
{
    m_degreeH = 90;
    m_angleIncreaseH = false;
    m_degreeV = 90;
    m_angleIncreaseV = false;

    PowerMonitor::getInstance()->powerOnWeapon();
    RadarMgr *radarMgr = RadarMgr::getInstance();
    int distance = 0;
    bool bLastFound = false;
    while (m_bStopFlag==false)
    {
        radarMgr->disableDetect();
        doPatrolH();
        doPatrolV();
        radarMgr->enableDetect();
        vTaskDelay(100 / portTICK_PERIOD_MS);  //wait for a while because the turret just moved. It may cause radar generate wrong result.
        distance = 0;
        if(radarMgr->scan(3, &distance)>=2 && distance > 0) {
            //warning: found target, prepare for shooting
            ESP_LOGW(TAG, "Found target at %dcm, prepare for shooting...", distance);
            if(bLastFound==false){
                SoundPlayer::getInstance()->playSpeach("EB-FIRTG"); 
                bLastFound = true;
            }
            //shooting
            SoundPlayer::getInstance()->play(SE_GUN_MCH); 

            for(int i = 0; i < 10; i++) {
                m_irmTransceiver.send(m_irmCode>>8, m_irmCode&0xFF);

                gpio_set_level(GPIO_TURRET_ALERT_LED, 1);
                if(m_bStopFlag==true)break;
                vTaskDelay(120 / portTICK_PERIOD_MS);

                gpio_set_level(GPIO_TURRET_ALERT_LED, 0);

                if(m_bStopFlag==true)break;
                vTaskDelay(120 / portTICK_PERIOD_MS);
            }
            gpio_set_level(GPIO_TURRET_ALERT_LED, 0);
        }else{
            bLastFound = false;
        }
    }

    PowerMonitor::getInstance()->powerOffWeapon();
    return 0;
}

void AutoTurret::proc(void *ctx)
{
    AutoTurret *handle = (AutoTurret*)ctx;
    handle->m_bIsRunningState = true;
    handle->doPatrol();
    handle->m_bIsRunningState = false;
    ESP_LOGI(TAG, "AutoTurret exiting...");
    vTaskDelete(NULL);
}

int AutoTurret::start()
{
    if(m_bIsRunningState == true) return ESP_FAIL;

    m_bStopFlag = false;
    xTaskCreate(AutoTurret::proc, "autoturret_task", AUTOTURRET_STACK_SIZE, (void*)this, 10, NULL);
    return 0;
}

int AutoTurret::stop()
{
    m_bStopFlag = true;
    while(m_bIsRunningState) {
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    setAngleV(90);
    setAngleH(90);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ledc_stop(LEDC_MODE, LEDC_CHANNEL_H, 0);
    ledc_stop(LEDC_MODE, LEDC_CHANNEL_V, 0);
    return 0;
}
