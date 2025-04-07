#pragma once
#include "IRMTransceiver.h"

class AutoTurret {
private:
    int m_degreeH;
    bool m_angleIncreaseH;
    int m_degreeV;
    bool m_angleIncreaseV;
    bool m_bStopFlag;
    bool m_bIsRunningState;
    IRMTransceiver m_irmTransceiver;
    uint16_t m_irmCode;
private:
    static void proc(void *ctx);
public:
    int init(void);
    int setAngleH(int degree);
    int setAngleV(int degree);
    int doPatrolH();
    int doPatrolV();
    int doPatrol();
    int start();
    int stop();
    void setIRMCode(uint16_t code){m_irmCode=code;};
public:
    static AutoTurret* getInstance();
};
