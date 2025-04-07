#pragma once
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

class RadarMgr {
private:
    bool m_bUARTDeviceAcquired;
    int buildOutgoingPacket(uint16_t cmd, uint16_t data1, uint16_t data2, uint16_t len);
    int extractCmdResponse(uint16_t *cmd, uint32_t* data);
    int sendCmdPacket(uint16_t len);

public:
    QueueHandle_t m_cmdQueue;
public:
    RadarMgr();
    int init();
    int acquireUARTDevice();
    int releaseUARTDevice();
    int getDelay();
    int setDelay(uint16_t data);
    int enableDetect();
    int disableDetect();
    int scan(int nTimes, int *distance);
public:
    static RadarMgr* getInstance();
};

