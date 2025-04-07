#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define SE_POWER_ON         "power_on"
#define SE_MARCH            "march"
#define SE_GUN_M1           "gun_m1"
#define SE_GUN_MCH          "gun_mch"

#define SOUND_VOLUME_MAX    3
#define SOUND_VOLUME_MID    -10
#define SOUND_VOLUME_MIN    -30

#define VOICE_TYPE_SOUND    0
#define VOICE_TYPE_SPEACH   1

#define SOUND_PLAY_PRI_NORMAL    0
#define SOUND_PLAY_PRI_HIGH_1    1
#define SOUND_PLAY_PRI_HIGH_2    2

typedef struct {
    char name[80];
    uint8_t voice_type;
}voice_cmd_t;

class SoundPlayer
{
private:
    QueueHandle_t m_cmdQueue;
    int8_t  m_nVolume;

    static void proc(void *ctx);
    int _play(char *name, bool bIsSpeach=false);
    int buildNum(char *buff, int num);

public:
    int init();
    int play(const char *name, uint8_t nHighPri=SOUND_PLAY_PRI_NORMAL, bool bIsSpeach=false, uint16_t nWaitTime=100);
    int playSpeach(uint16_t cmd_id);
    int playSpeach(const char *cmd_str, uint16_t nWaitTime=100);
    int playSpeachWithNum(const char *name1, int num, const char *name2=NULL);
    bool isPlaying();
    int stop();
    void setVolume(int8_t vol) { m_nVolume = vol; };

public:
    static SoundPlayer* getInstance();
};