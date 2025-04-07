#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "TargetServo.h"
#include "PowerMonitor.h"


static const char *TAG = "TARGET_SERVO";
static TargetServo g_targetServo;

#define LEDC_TIMER              LEDC_TIMER_1
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (GPIO_NUM_38) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_2
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT // Set duty resolution to 13 bits
//#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (50) // Frequency in Hertz. Set frequency at 50 Hz

/* Warning:
 * For ESP32, ESP32S2, ESP32S3, ESP32C3 targets,
 * when LEDC_DUTY_RES selects the maximum duty resolution (i.e. value equal to SOC_LEDC_TIMER_BIT_WIDTH),
 * 100% duty cycle is not reachable (duty cannot be set to (2 ** SOC_LEDC_TIMER_BIT_WIDTH)).
 */


int calculatePWM(int degree)
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

int TargetServo::init(void)
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
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = LEDC_OUTPUT_IO,
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    m_tmLastActionTime = 0;
    return 0;
}


#define CAL_DUTY(x)    ((uint16_t)(8192 * (x/(1.0*1000.0/LEDC_FREQUENCY))))
#define ANGLE_N90      CAL_DUTY(0.5)
#define ANGLE_N45      CAL_DUTY(1)
#define ANGLE_0        CAL_DUTY(1.5)
#define ANGLE_P45      CAL_DUTY(2)
#define ANGLE_P90      CAL_DUTY(2.5)

int TargetServo::setDown(void)
{
    PowerMonitor::getInstance()->powerOnTgtServo();
    
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, calculatePWM(0)));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

    m_tmLastActionTime = esp_timer_get_time();
    return 0;
}

int TargetServo::setUp(void)
{
    PowerMonitor::getInstance()->powerOnTgtServo();

    ESP_LOGW(TAG, "Target servo duty is set to: %d.", calculatePWM(90));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, calculatePWM(90)));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

    m_tmLastActionTime = esp_timer_get_time();
    return 0;
}

void TargetServo::stop()
{
    if(m_tmLastActionTime!=0 && esp_timer_get_time()-m_tmLastActionTime < 1000*1000) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    PowerMonitor::getInstance()->powerOffTgtServo();
}

TargetServo * TargetServo::getInstance()
{
  return &g_targetServo;
}
