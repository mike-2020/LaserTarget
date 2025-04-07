#pragma once
#include <stdint.h>


class ASRCOmmandProc {
private:
    static int m_bUartInit;
    static void proc(void *ctx);
    
    int processASRCmd(uint16_t cmd_id);
public:
    int init();
    static int sendCmd(uint8_t cmd_type, uint16_t cmd_id);
    int runCmd(uint8_t cmd_type, uint8_t* cmd_data, size_t len);
};

#define CMD_TYPE_CTL        1
#define CMD_TYPE_ASR        2




#define ASR_CMD_PLAY_AUDIO      1
#define ASR_CMD_1SHOOT          2
#define ASR_CMD_2SHOOT          3
#define ASR_CMD_8SHOOT          4
#define ASR_CMD_ATTACK_SHOOT    5
#define ASR_CMD_ASK_EXIT        6
#define ASR_CMD_MAX_VOLUME      7
#define ASR_CMD_MIN_VOLUME      8
#define ASR_CMD_MID_VOLUME      9
#define ASR_CMD_REPORT_POWER    10
#define ASR_CMD_TMLIMIT_SHOOT   11
#define ASR_CMD_RANDOM_SHOOT    12
#define ASR_CMD_VOICE_HELP      13
#define ASR_CMD_AIM_HELP        14





////////////////////////////////////////////////////////////
#define CMD_TYPE_WAKEUP         3
#define WAKEUP_CMD_ENTER        1
#define WAKEUP_CMD_EXIT         2
///////////////////////////////////////////////////////////

#define CTL_CMD_EN_ASR          1
#define CTL_CMD_DN_ASR          2
//#define CTL_CMD_EN_WEAPON_PWR   3
//#define CTL_CMD_DN_WEAPON_PWR   4
#define CTL_CMD_ASR_SLEEP       5


#define VOICE_WAKE_RESPONSE     "RS-WAKE"
#define VOICE_EXIT_RESPONSE     "RS-EXIT"
#define VOICE_8SHOOT_START      "RS-8STST"
#define VOICE_2SHOOT_START      "RS-2STST"
#define VOICE_1SHOOT_START      "RS-1STST"
#define VOICE_TMLIMIT_START     "RS-TLST"
#define VOICE_RNDSHOOT_START    "RS-RDST"


#define VOICE_8SHOOT_SUCCESS    "EB-8STOK"
#define VOICE_2SHOOT_SUCCESS    "EB-2STOK"
#define VOICE_SHOOT_MISS        "EB-MISS"
#define VOICE_SHOOT_WRONG       "EB-WRON"
#define VOICE_COUNT_BACK        "EB-CTBK"
#define VOICE_POWER_REPORT      "EB-PWRPT"

#define VOICE_WELCOME           "WELCOME"
#define VOICE_SUCCESS_1         "SUCCESS1"
#define VOICE_SUCCESS_2         "SUCCESS2"

#define VOICE_MAX_VOLUME        "CV-MAXVL"
#define VOICE_MIN_VOLUME        "CV-MINVL"
#define VOICE_MID_VOLUME        "CV-MIDVL"
#define VOICE_HELP              "CV-HELP"

