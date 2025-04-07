#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"


class LR_IS0803;
class LaserReceiver {
protected:

public:
    bool m_bEnableState;
public:
    void disable() { m_bEnableState = false; };
    void enable() { m_bEnableState = true; };
    static LR_IS0803* getInstance();
};


class LR_IS0803 : public LaserReceiver{
private:
public:
    QueueHandle_t m_cmdQueue;
    int init();
    int getCmd(uint32_t waitTime);
    void resetQueue();
    static int64_t getLastShootTime();
};

