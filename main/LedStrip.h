#pragma once
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

//#define RGB_COLOR(r, g, b)      ((uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r )
//G B R
#define RGB_COLOR(r, g, b)  {g, b, r}
#define LED_COLOR_NONE      RGB_COLOR(0, 0, 0)
#define LED_COLOR_RED       RGB_COLOR(255, 0, 0)
#define LED_COLOR_GREEN     RGB_COLOR(0, 255, 0)
#define LED_COLOR_BLUE      RGB_COLOR(0, 0, 255)

#define LED_CMD_BLINK_FLOW      1
#define LED_CMD_BLINK_RANDOM    2
#define LED_CMD_OFF_ONE         3
#define LED_CMD_ON_ONE          4
#define LED_CMD_ON_ALL          5
#define LED_CMD_OFF_ALL         6
#define LED_CMD_OFF_HALF        7
#define LED_CMD_ON_ONE_MORE     8
#define LED_CMD_STOP            9
#define LED_CMD_OFF_ONE_MORE    10



typedef struct ledcolor
{
    uint8_t g;
    uint8_t b;
    uint8_t r;
} ledcolor;

class LEDStrip
{
private:
    int m_nLedNum;
    uint8_t *m_led_strip_pixels;
    QueueHandle_t m_cmdQueue;
    ledcolor m_curUsedColor;
    bool m_bStopFlag;
    bool m_bIsRunning;

    void update();
    inline ledcolor getRandomColor();
    static void proc(void *ctx);

public:
    int init();
    void stop();
    void clear();
    void blinkAll(uint16_t nTimes, uint16_t interval);
    void setColor(int idx, ledcolor color);
    void setColorAll(ledcolor color);
    void blinkFlow(uint16_t nTimes, uint16_t interval);
    void blinkAllRandomColor();
    void turnOnAll();
    void turnOffHalf();
    void turnOnOneMore();
    void turnOffOneMore();

    static LEDStrip* getInstance();
    int sendCmd(uint8_t cmd, bool bResetQueue=false);
};