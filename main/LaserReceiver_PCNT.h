#pragma once
#include "stdint.h"

class LaserReceiver{
public:
    virtual int init() =0;
    virtual int getCmd(uint32_t waitTime)=0;
    //static LR_IS0803* getInstance();
};


class LR_PCNT : LaserReceiver{
private:
public:
    QueueHandle_t m_cmdQueue;
    int init();
    int getCmd(uint32_t waitTime);
    //static LR_IS0803* getInstance();
};



