#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "led_strip_encoder.h"
#include "LedStrip.h"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define LED_GPIO GPIO_NUM_39
#define LED_NUM 8
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

static const char *TAG = "LED_STRIP";
static LEDStrip g_LEDStrip;

#define LED_COLOR_TABLE_SIZE     3
static ledcolor led_color_table[] = {LED_COLOR_RED, LED_COLOR_BLUE, LED_COLOR_GREEN};

static void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

static rmt_channel_handle_t LED_CH = NULL; // LED的通道
static rmt_encoder_handle_t LED_ENCODE = NULL; // LED的编码器
static int led_rmt_init(int pin_num)
{
    rmt_tx_channel_config_t LED_CH_CONFIG =
    {
        .gpio_num = (gpio_num_t)pin_num,            // 配置rmt输出引脚
        .clk_src = RMT_CLK_SRC_DEFAULT, // 选择时钟资源：默认资源
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,      // 频率为10MHz，1个tick是0.1us
        .mem_block_symbols = 48,        // 增大块的大小可以使 LED 减少闪烁？
        .trans_queue_depth = 4,         // 设置后台待处理的事务数量？
    };
    rmt_new_tx_channel(&LED_CH_CONFIG, &LED_CH);

    ESP_LOGI(TAG, "Install led strip encoder");
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &LED_ENCODE));

    // 使能RMT
    rmt_enable(LED_CH);

    return 0;
}


ledcolor LEDStrip::getRandomColor()
{
    int n = esp_random()%LED_COLOR_TABLE_SIZE;
    return led_color_table[n];
}

void LEDStrip::update()
{
    // 配置发送
    rmt_transmit_config_t RMT_TRANSMIT_CONFIG = {.loop_count = 0}; // 不循环发送

    // 发送数据
    rmt_transmit(LED_CH, LED_ENCODE, m_led_strip_pixels, m_nLedNum * 3, &RMT_TRANSMIT_CONFIG);
    rmt_tx_wait_all_done(LED_CH, portMAX_DELAY);
}


int LEDStrip::init()
{
    m_nLedNum = LED_NUM;
    m_bStopFlag = false;
    m_bIsRunning = false;
    //ws28xx_init(LED_GPIO, WS2812B, LED_NUM, &ws2812_buffer);
    led_rmt_init(LED_GPIO);

    m_led_strip_pixels = (uint8_t*) malloc(m_nLedNum * 3);
    memset(m_led_strip_pixels, 0, m_nLedNum * 3);
    memset(&m_curUsedColor, 0, sizeof(ledcolor));

    m_cmdQueue = xQueueCreate(2, sizeof(uint8_t));
    xTaskCreate(LEDStrip::proc, "ledstrip_task", 1024+512, (void*)this, 10, NULL);
    return 0;
}

void LEDStrip::clear()
{
    memset(m_led_strip_pixels, 0, m_nLedNum * 3);
    update();
}

void LEDStrip::setColor(int idx, ledcolor color)
{

    //led_update();
}

void LEDStrip::setColorAll(ledcolor color)
{
    for(int i = 0; i < m_nLedNum; i++) {
        m_led_strip_pixels[i * 3 + 0] = color.g;
        m_led_strip_pixels[i * 3 + 1] = color.b;
        m_led_strip_pixels[i * 3 + 2] = color.r;
    }
    update();
}

void LEDStrip::blinkAllRandomColor()
{
    m_bStopFlag = false;
    m_bIsRunning= true;
    for(int i=0; i< 5; i++)
    {
        if(m_bStopFlag==true) break;
        setColorAll(getRandomColor());
        vTaskDelay(pdMS_TO_TICKS(300));
        clear();
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    clear();
    m_bIsRunning= false;
}

void LEDStrip::blinkAll(uint16_t nTimes, uint16_t interval)
{
    for(int i=0; i< nTimes; i++)
    {
        clear();
        setColorAll(LED_COLOR_RED);
        vTaskDelay(pdMS_TO_TICKS(interval));
        setColorAll(LED_COLOR_GREEN);
        vTaskDelay(pdMS_TO_TICKS(interval));
        setColorAll(LED_COLOR_BLUE);
        vTaskDelay(pdMS_TO_TICKS(interval));
    }
    clear();
}

void LEDStrip::blinkFlow(uint16_t nTimes, uint16_t interval)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    uint16_t hue = 0;
    uint16_t start_rgb = 0;

    m_bStopFlag = false;
    m_bIsRunning= true;
    while(nTimes > 0 && m_bStopFlag==false)
    {
        for (int i = 0; i < 3; i++) {
            if(m_bStopFlag==true) break;
            for (int j = i; j < m_nLedNum; j += 3) {
                // Build RGB pixels
                hue = j * 360 / m_nLedNum + start_rgb;
                led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
                m_led_strip_pixels[j * 3 + 0] = green;
                m_led_strip_pixels[j * 3 + 1] = blue;
                m_led_strip_pixels[j * 3 + 2] = red;
            }
            // Flush RGB values to LEDs
            update();
            vTaskDelay(pdMS_TO_TICKS(interval));
            clear();
            vTaskDelay(pdMS_TO_TICKS(interval));
        }
        start_rgb += 60;
    
        nTimes--;
    }
    clear();
    m_bIsRunning = false;
}

void LEDStrip::turnOnAll()
{
    setColorAll(getRandomColor());
}

void LEDStrip::turnOffHalf()
{
    uint8_t nProcessedCount = 0;
    uint8_t nTurnOffCount = 0;
    int i = esp_random()%LED_NUM;       //randomly select the starting LED to turn off
    while(nProcessedCount < LED_NUM && nTurnOffCount < LED_NUM/2) {
        uint8_t *p = &m_led_strip_pixels[i*3];
        if(p[0]!=0 || p[1]!=0 || p[2]!=0) {
            p[0] = 0;
            p[1] = 0;
            p[2] = 0;
            nTurnOffCount++;
        }
        nProcessedCount++;
        i++;
        if(i>=LED_NUM) i = 0;
    }
    update();
}

void LEDStrip::turnOnOneMore()
{
    bool found = false;
    if(m_curUsedColor.g==0 && m_curUsedColor.b==0 && m_curUsedColor.r==0) {
        m_curUsedColor = getRandomColor();
    }

    for(int i = 0; i < m_nLedNum; i++) {
        uint8_t *p = &m_led_strip_pixels[i*3];
        if(p[0]==0 && p[1]==0 && p[2]==0) {
            p[0] = m_curUsedColor.g;
            p[1] = m_curUsedColor.b;
            p[2] = m_curUsedColor.r;

            found = true;
            break;
        }
    }

    if(found==true) {
        update();
    }else{              //all LEDs have been turned on, then blink it
        clear();
        vTaskDelay(pdMS_TO_TICKS(500));
        setColorAll(m_curUsedColor);
    }

}

void LEDStrip::turnOffOneMore()
{
    bool found = false;

    for(int i = 0; i < m_nLedNum; i++) {
        uint8_t *p = &m_led_strip_pixels[i*3];
        if(p[0]!=0 || p[1]!=0 || p[2]!=0) {
            p[0] = 0;
            p[1] = 0;
            p[2] = 0;

            found = true;
            break;
        }
    }

    if(found==true) {
        update();
    }
}


LEDStrip* LEDStrip::getInstance()
{
    return &g_LEDStrip;
}

void LEDStrip::proc(void *ctx)
{
    LEDStrip *handle = (LEDStrip*)ctx;
    uint8_t cmd = 0;
    while(1) {
        if (xQueueReceive(handle->m_cmdQueue, &cmd, pdMS_TO_TICKS(1000))!=pdTRUE){
            continue;
        }
        switch(cmd) {
            case LED_CMD_BLINK_FLOW:
                handle->blinkFlow(59999, 300);
                break;
            case LED_CMD_BLINK_RANDOM:
                handle->blinkAllRandomColor();
                break;
            case LED_CMD_ON_ALL:
                handle->turnOnAll();
                break;
            case LED_CMD_OFF_ALL:
                handle->clear();
                break;
            case LED_CMD_OFF_HALF:
                handle->turnOffHalf();
                break;
            case LED_CMD_ON_ONE_MORE:
                handle->turnOnOneMore();
                break;
            case LED_CMD_OFF_ONE_MORE:
                handle->turnOffOneMore();
                break;

        }
    }
}

int LEDStrip::sendCmd(uint8_t cmd, bool bResetQueue)
{
    if(bResetQueue==true) {
        xQueueReset(m_cmdQueue);
    }

    BaseType_t rc = xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(100));
    if(rc!=pdTRUE) {
        ESP_LOGE(TAG, "Failed to send command: %u.", cmd);
        return ESP_FAIL;
    }
    return ESP_OK;
}


void LEDStrip::stop()
{
    m_bStopFlag = true;

    while(m_bIsRunning == true)vTaskDelay(pdMS_TO_TICKS(10));
}
