#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "PowerMonitor.h"
#include "LaserReceiver_IS0803.h"

#define GPIO_LASER_RECV    GPIO_NUM_40
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_LASER_RECV)


static LR_IS0803 g_laserRecv;
static int64_t last_trigger_time = 0;
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    LR_IS0803 *handle = (LR_IS0803*) arg;
    uint32_t cmd = 1;
    int64_t tm = esp_timer_get_time();
    if(tm - last_trigger_time < 500*1000) {     //ignore new triggers in 0.5s
        return;
    }
    last_trigger_time = tm;
    if(handle->m_bEnableState)
        xQueueSendFromISR(handle->m_cmdQueue, &cmd, NULL);
}

int64_t LR_IS0803::getLastShootTime()
{
    return last_trigger_time;
}

int LR_IS0803::init()
{
    gpio_config_t io_conf = {};

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)1;
    gpio_config(&io_conf);

    //change gpio interrupt type for one pin
    gpio_set_intr_type(GPIO_LASER_RECV, GPIO_INTR_POSEDGE);

    //create a queue to handle gpio event from isr
    m_cmdQueue = xQueueCreate(2, sizeof(uint32_t));
    //start gpio task
    //xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_LASER_RECV, gpio_isr_handler, (void*) this);

    m_bEnableState = true;
    return 0;
}

int LR_IS0803::getCmd(uint32_t waitTime)
{
    uint32_t cmd = 1;
    if (xQueueReceive(m_cmdQueue, &cmd, pdMS_TO_TICKS(waitTime))!=pdTRUE) return -1;
    PowerMonitor::getInstance()->updateInputActivityTime();
    return cmd;
}

void LR_IS0803::resetQueue()
{
    xQueueReset(m_cmdQueue);
}

LR_IS0803* LaserReceiver::getInstance()
{
    return &g_laserRecv;
}
