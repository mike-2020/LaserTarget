#pragma once
#include <stdint.h>


typedef struct 
{
    uint16_t uid;
    uint8_t dev_type;
    uint8_t addr[6];
    uint16_t conn_handle;
}dev_info_t;

typedef struct 
{
    uint16_t src;
    uint16_t dst;
    uint16_t cmd;
    uint16_t val;
}ble_cmd_t;


class BLEMgr {
private:

    QueueHandle_t m_cmdSendQueue;
    uint16_t m_uidStartPoint;
private:
    static void proc(void *ctx);

public:
    BLEMgr();
    int start();
    int sendCmd(ble_cmd_t& cmd);
    int pingAll();
    void recvData(uint8_t* data, size_t len);
    int createDevNode();
    void setAimHelp();
    static BLEMgr* getInstance();
};






#define MAKE_RSP_CMD(X) (X|0x8000U)
#define BLE_CMD_GET_UID         1
#define BLE_CMD_PING            2
#define BLE_CMD_SHOOT           3
#define BLE_CMD_AMMO_OUT        4
#define BLE_CMD_RELOAD          5
#define BLE_CMD_AIM             6
#define BLE_CMD_BESHOT          7
