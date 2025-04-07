#pragma once


#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "EpicBase.h"

#define EPIC_CMD_SWITCH_EPIC    1
#define EPIC_CMD_BESHOT_INFORM  2

#define EPICADVATTACK_IRM_CODE      0x1122

class EpicMgr{
private:
    QueueHandle_t m_cmdQueue;
    EpicBase *m_curEpic;
    uint16_t m_nHitTargetCount;   //this is about this device hit user, not user hit this device.
private:
    int switchEpic(uint8_t idx);
    int informHitTarget(uint16_t code);
public:
    void run();
    int sendCmd(uint8_t key, uint16_t val);
    uint16_t getHitCount() { return m_nHitTargetCount; };
    static EpicMgr* getInstance();
};

