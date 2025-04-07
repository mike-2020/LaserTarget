#pragma once
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


typedef struct {
    uint8_t src;
    uint8_t tgt;
    uint8_t key;
    uint16_t val;
}epic_cmd_t;

class EpicBase {
private:

    QueueHandle_t m_cmdQueue;
    
    uint8_t m_nWordsIdx;
protected:
    bool m_bIsRunning;
    bool m_bStopFlag;
    uint8_t m_nEpicID;
    TaskHandle_t m_taskHandle;
    void playEncourageWords();
    int readNVSPair(const char *key, char *val, size_t len);
    int readNVSPair(const char *key);
    int saveNVSPair(const char *key, const char *val);
    int saveNVSPair(const char *key, int val);
public:
    EpicBase();
    virtual ~EpicBase();
    int init();
    virtual void start() = 0;
    int stop();
    uint8_t getEpicID() { return m_nEpicID; };
    bool isStopped() { return (m_bStopFlag==true && m_bIsRunning==false); };
};

class EpicSingleShot : public EpicBase {
private:
    static void proc(void *ctx);

public:
    EpicSingleShot();
    virtual void start();
};


class EpicDoubleShot : public EpicBase {
private:
    static void proc(void *ctx);

public:
    EpicDoubleShot();
    virtual void start();
};


class EpicEightShot : public EpicBase {
private:
    static void proc(void *ctx);
    void checkHistoryRecord(int nTime);
public:
    EpicEightShot();
    virtual void start();
};


class EpicTimeLimit : public EpicBase {
private:
    uint8_t m_nInterval;
    static void proc(void *ctx);
    void checkHistoryRecord(int shotCount);

public:
    EpicTimeLimit(uint8_t tm=10);
    virtual void start();
};

class EpicRandomShoot : public EpicBase {
private:
    static void proc(void *ctx);
    void makeRandomCase(uint32_t nTime, bool *pInRecvShootPeriod);

public:
    EpicRandomShoot();
    virtual void start();
};

class EpicAdvAttack : public EpicBase {
private:
    static void proc(void *ctx);
    void makeRandomCase(uint32_t nTime, bool *pInRecvShootPeriod);

public:
    EpicAdvAttack();
    virtual void start();
};



#define EPIC_SINGLESHOT     1
#define EPIC_DOUBLESHOT     2
#define EPIC_EIGHTSHOT      3
#define EPIC_TMLIMIT        4
#define EPIC_RANDOMSHOOT    5
#define EPIC_ADVATTACK      6







