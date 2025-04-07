#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "EpicBase.h"
#include "EpicMgr.h"
#include "PowerMonitor.h"
#include "SoundPlayer.h"
#include "ASRCOmmandProc.h"


static const char *TAG = "EPIC_MANAGER";
static EpicMgr g_epicMgr;

#define QUEUE_WAIT_TIME     200     //unit=ms
#define PLAY_HELP_TIME      ((30*1000)/QUEUE_WAIT_TIME)

void EpicMgr::run()
{
    int nLoopCount = 0;
    m_cmdQueue = xQueueCreate(2, sizeof(epic_cmd_t));

    m_curEpic = new EpicSingleShot();       //start default epic
    m_curEpic->start();
    epic_cmd_t cmd;
    while(1) {
        nLoopCount++;

        if (xQueueReceive(m_cmdQueue, &cmd, pdMS_TO_TICKS(QUEUE_WAIT_TIME))==pdTRUE) {
            ESP_LOGI(TAG, "Received command %u:%u from %u.", cmd.key, cmd.val, cmd.src);
            if(cmd.key==EPIC_CMD_SWITCH_EPIC) {
                switchEpic(cmd.val);
            }else if(cmd.key == EPIC_CMD_BESHOT_INFORM) {
                informHitTarget(cmd.val);
            }
        }else{
            if(m_curEpic!=NULL && m_curEpic->isStopped()==true) {
                delete m_curEpic;
                m_curEpic = NULL;
            }
        }

        if(nLoopCount==PLAY_HELP_TIME && PowerMonitor::getInstance()->getLastInputActivityTime()==0) {
            SoundPlayer::getInstance()->playSpeach(VOICE_HELP); 
        }
    }
}

int EpicMgr::switchEpic(uint8_t idx)
{
    if(m_curEpic!=NULL && m_curEpic->getEpicID()== idx) return ESP_OK;
    if(m_curEpic!=NULL) {
        m_curEpic->stop();
        delete m_curEpic;
        m_curEpic = NULL;
    }
    m_nHitTargetCount = 0;
    switch(idx) {
        case EPIC_SINGLESHOT:
            m_curEpic = new EpicSingleShot();
            break;
        case EPIC_DOUBLESHOT:
            m_curEpic = new EpicDoubleShot();
            break;
        case EPIC_EIGHTSHOT:
            m_curEpic = new EpicEightShot();
            break;
        case EPIC_TMLIMIT:
            m_curEpic = new EpicTimeLimit();
            break;
        case EPIC_RANDOMSHOOT:
            m_curEpic = new EpicRandomShoot();
            break;
        case EPIC_ADVATTACK:
            m_curEpic = new EpicAdvAttack();
            break;
    }
    if(m_curEpic!=NULL) {
        m_curEpic->start();
    }

    return ESP_OK;
}

int EpicMgr::informHitTarget(uint16_t code)
{
    ESP_LOGW(TAG, "informHitTarget code = 0x%X.", code);
    if(EPICADVATTACK_IRM_CODE!=code) return -1;
    m_nHitTargetCount++;
    SoundPlayer::getInstance()->playSpeach("EB-HITYU"); 
    return 0;
}

int EpicMgr::sendCmd(uint8_t key, uint16_t val)
{
    epic_cmd_t cmd;
    cmd.key = key;
    cmd.val = val;
    BaseType_t rc = xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(10));
    if(rc!=pdTRUE) {
        ESP_LOGE(TAG, "Failed to send sound play command: %u.", key); 
        return ESP_FAIL;
    }
    return ESP_OK;
}

EpicMgr* EpicMgr::getInstance()
{
    return &g_epicMgr;
}
