#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/def.h"
#include "ble_net.h"
#include "BLEMgr.h"
#include "LaserReceiver_IS0803.h"
#include "SoundPlayer.h"
#include "EpicMgr.h"


#define BLEMGR_MAX_DEV_NUM       3
static BLEMgr g_ble_mgr;
static dev_info_t m_clientUIDList[BLEMGR_MAX_DEV_NUM];
static QueueHandle_t m_cmdRecvQueue;
static int m_uidStartPoint = 100;
static const char *TAG = "BLE_MGR";

BLEMgr* BLEMgr::getInstance()
{
    return &g_ble_mgr;
}

BLEMgr::BLEMgr()
{
    for(int i=0; i< BLEMGR_MAX_DEV_NUM; i++) {
        m_clientUIDList[i].conn_handle = 0xFFFF;
    }
}

int BLEMgr::start()
{
    m_cmdRecvQueue = xQueueCreate(5, sizeof(ble_cmd_t));
    ble_net_init();

    xTaskCreate(BLEMgr::proc, "ble_task", 1024*3, (void*)this, 10, NULL);
    return 0;
}

void BLEMgr::setAimHelp()
{
    ble_cmd_t cmd;
    cmd.src = 0xFFFF;
    cmd.dst = 0;
    cmd.cmd = BLE_CMD_AIM;
    cmd.val = 0;
    for(int i=0; i< BLEMGR_MAX_DEV_NUM; i++) {
        if((m_clientUIDList[i].uid)==0) continue;
        cmd.dst = (m_clientUIDList[i].uid);
        cmd.val = (m_clientUIDList[i].uid);
        ble_send_data(m_clientUIDList[i].conn_handle, (uint8_t*)&cmd, sizeof(ble_cmd_t));
    }
    SoundPlayer::getInstance()->playSpeach("EB-OK");
}


int BLEMgr::sendCmd(ble_cmd_t& cmd)
{
    return 0;
}

int BLEMgr::pingAll()
{
    ble_cmd_t cmd;
    for(int i=0; i< BLEMGR_MAX_DEV_NUM; i++) {
        if((m_clientUIDList[i].uid)==0) continue;
        cmd.src = (0xFFFF);
        cmd.dst = (m_clientUIDList[i].uid);
        cmd.cmd = (BLE_CMD_PING);
        cmd.val = (m_clientUIDList[i].uid);
        ble_send_data(m_clientUIDList[i].conn_handle, (uint8_t*)&cmd, sizeof(ble_cmd_t));
    }
    return 0;
}

void BLEMgr_rspPing(uint16_t conn_handle, ble_cmd_t *cmd)
{
    ESP_LOGI(TAG, "Ping echo is received from %u.", cmd->src);
}

extern "C" int BLEMgr_getDevUID(const uint8_t *addr, size_t len)
{
    for(int i=0; i< BLEMGR_MAX_DEV_NUM; i++)
    {
        if(memcmp(m_clientUIDList[i].addr, addr, len)==0) {
            return m_clientUIDList[i].uid;
        }
    }
    return -1;
}

extern "C" int BLEMgr_getDevUIDByHandle(uint16_t conn_handle)
{
    for(int i=0; i< BLEMGR_MAX_DEV_NUM; i++)
    {
        if(m_clientUIDList[i].conn_handle == conn_handle) {
            return m_clientUIDList[i].uid;
        }
    }
    return -1;
}

void BLEMgr_rspGetUID(uint16_t conn_handle)
{
    int uid = BLEMgr_getDevUIDByHandle(conn_handle);
    ble_cmd_t cmd;
    cmd.src = (0xFFFF);
    cmd.dst = (uid);
    cmd.cmd = MAKE_RSP_CMD(BLE_CMD_GET_UID);
    cmd.val = (uid);
    ble_send_data(conn_handle, (uint8_t*)&cmd, sizeof(ble_cmd_t));
}

extern "C" int BLEMgr_createDevNode(uint16_t conn_handle, uint8_t *addr, size_t len)
{
    for(int i=0; i< BLEMGR_MAX_DEV_NUM; i++)
    {
        if(memcmp(m_clientUIDList[i].addr, addr, len)==0) {
            return m_clientUIDList[i].uid;
        }
        if(m_clientUIDList[i].uid!=0) continue;

        m_clientUIDList[i].conn_handle = conn_handle;
        m_clientUIDList[i].uid = m_uidStartPoint++;
        memcpy(m_clientUIDList[i].addr, addr, sizeof(m_clientUIDList[i].addr));
        ESP_LOGW(TAG, "Device node has been created. Address = %X-%X-%X-%X-%X-%X.", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        return m_clientUIDList[i].uid;
    }
    return -1;
}

extern "C" void BLEMgr_recvData(uint8_t* data, size_t len, uint16_t conn_handle)
{
    ble_cmd_t cmd;
    cmd.src = (*((uint16_t*)data));
    cmd.dst = (*((uint16_t*)&data[2]));
    cmd.cmd = (*((uint16_t*)&data[4]));
    cmd.val = (*((uint16_t*)&data[6]));

    ESP_LOGI(TAG, "Recieved data from %d.", cmd.src);

    switch(cmd.cmd) {
        case BLE_CMD_GET_UID:
            BLEMgr_rspGetUID(conn_handle);
            break;
        case MAKE_RSP_CMD(BLE_CMD_PING):
            BLEMgr_rspPing(conn_handle, &cmd);
            break;
        case BLE_CMD_SHOOT:
        case BLE_CMD_AMMO_OUT:
        case BLE_CMD_RELOAD:
        default:
            int uid = BLEMgr_getDevUIDByHandle(conn_handle);
            //if(cmd.src != uid) {
            //    ESP_LOGW(TAG, "Invalid device connection: conn_handle=%d, src=%u, cmd=0x%X", conn_handle, cmd.src, cmd.cmd);
            //    return;
            //}
            xQueueSend(m_cmdRecvQueue, &cmd, pdMS_TO_TICKS(100));
    }
}

extern "C" void BLEMgr_delDevNode(uint16_t conn_handle)
{
    for(int i=0; i< BLEMGR_MAX_DEV_NUM; i++)
    {
        if(m_clientUIDList[i].conn_handle == conn_handle) {
            uint8_t *addr = m_clientUIDList[i].addr;
            ESP_LOGW(TAG, "Device node is being removed. Address = %X:%X:%X:%X:%X:%X.", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            memset(&m_clientUIDList[i], 0, sizeof(dev_info_t));
            m_clientUIDList[i].conn_handle = 0xFFFF;
            break;
        }
    }
}


void BLEMgr_procCmdShoot(ble_cmd_t *cmd)
{
    ESP_LOGI(TAG, "SHOOT command is received from %u.", cmd->src);
    int64_t last_shoot_time = LR_IS0803::getInstance()->getLastShootTime();
    int64_t tm = esp_timer_get_time();
    if(abs(tm - last_shoot_time)>300*1000) {
        ESP_LOGW(TAG, "tm - last_shoot_time = %d", (int)(tm - last_shoot_time));
        SoundPlayer::getInstance()->playSpeach("EB-MISS");
    }
}

void BLEMgr_procCmdAmmoOut(ble_cmd_t *cmd)
{
    ESP_LOGI(TAG, "AMMO_OUT command is received from %u.", cmd->src);
    //SoundPlayer::getInstance()->playSpeach("EB-AOUT");
}

void BLEMgr_procCmdReload(ble_cmd_t *cmd)
{
    ESP_LOGI(TAG, "RELOAD command is received from %u.", cmd->src);
    //SoundPlayer::getInstance()->playSpeach("EB-RLOAD");
}

void BLEMgr_procCmdBeShot(ble_cmd_t *cmd)
{
    ESP_LOGI(TAG, "BE_SHOT command is received from %u, val = 0x%X.", cmd->src, cmd->val);
    EpicMgr::getInstance()->sendCmd(EPIC_CMD_BESHOT_INFORM, cmd->val);
}

void BLEMgr::proc(void *ctx)
{
    BLEMgr *handle = (BLEMgr*)ctx;
    uint32_t nLoopCount = 0;
    ble_cmd_t cmd;
    while(1) {
        if (xQueueReceive(m_cmdRecvQueue, &cmd, pdMS_TO_TICKS(1000))!=pdTRUE){
            nLoopCount++;
            if(nLoopCount==10) {
                nLoopCount = 0;
                handle->pingAll();
            }
            continue;
        }
        switch(cmd.cmd) {
            case BLE_CMD_SHOOT:
                BLEMgr_procCmdShoot(&cmd);
                break;
            case BLE_CMD_AMMO_OUT:
                BLEMgr_procCmdAmmoOut(&cmd);
                break;
            case BLE_CMD_RELOAD:
                BLEMgr_procCmdReload(&cmd);
                break;
            case BLE_CMD_BESHOT:
                BLEMgr_procCmdBeShot(&cmd);
                break;
        }
    }
}