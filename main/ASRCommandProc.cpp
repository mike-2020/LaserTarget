#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "SoundPlayer.h"
#include "EpicMgr.h"
#include "ASRCOmmandProc.h"
#include "PowerMonitor.h"
#include "SegDisplay.h"
#include "BLEMgr.h"


#define ECHO_TEST_TXD (GPIO_NUM_6)
#define ECHO_TEST_RXD (GPIO_NUM_5)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)
#define ECHO_UART_PORT_NUM      ((uart_port_t)(1))
#define ECHO_UART_BAUD_RATE     (38400)
#define ECHO_TASK_STACK_SIZE    1024*3
#define BUF_SIZE (1024)

static const char *TAG = "ASR_CMD_PROC";

int ASRCOmmandProc::m_bUartInit = 0;

void uart_cmd_task(void *ctx)
{
    uint8_t buff[8];
    int recv_len = 0;
    int i = 0;
    ASRCOmmandProc *handle = (ASRCOmmandProc*)ctx;
    while (1)
    {
        uint8_t c = '\0';
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, &c, 1, 20 / portTICK_PERIOD_MS);
        if(len <= 0) continue;
        recv_len++;
        switch(recv_len) {
            case 1:
                if(c != 0xA5) { recv_len = 0; }
                continue;
            case 2:
                if(c != 0xFA) { recv_len = 0; }
                continue;
            case 6:
                if(c != 0xFB ) { }
                recv_len = 0;
                continue;
            case 3:
            case 4:
            case 5:
                buff[i++] = c;
                break;
            default:
                recv_len = 0;
        }

        if(i>=3) {
            handle->runCmd(buff[0], &buff[1], 2);
            i = 0;
        }
    }
    
}


int ASRCOmmandProc::init()
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    xTaskCreate(uart_cmd_task, "uart_cmd_task", ECHO_TASK_STACK_SIZE, (void*)this, 10, NULL);

    m_bUartInit = 1;
    ESP_LOGI(TAG, "ASRCOmmandProc has been initialized.");
    return 0;
}



int ASRCOmmandProc::runCmd(uint8_t cmd_type, uint8_t* cmd_data, size_t len)
{
    uint8_t cmd[2];
    cmd[0] = cmd_data[1];
    cmd[1] = cmd_data[0];
    uint16_t cmd_id = *((uint16_t*)(cmd));

    ESP_LOGW(TAG, "Received command type %d (cmd_id=%u).", cmd_type, cmd_id);
    if(cmd_type == CMD_TYPE_CTL) {

    }else if(cmd_type == CMD_TYPE_WAKEUP && cmd_id==WAKEUP_CMD_EXIT) {
        //SoundPlayer::getInstance()->playSpeach(VOICE_EXIT_RESPONSE);       //exit wakeup. DONOT play voice for this event because it can interrupt other ongoing sound playing

    }else if(cmd_type == CMD_TYPE_WAKEUP && cmd_id==WAKEUP_CMD_ENTER) {
        SoundPlayer::getInstance()->playSpeach(VOICE_WAKE_RESPONSE); 

    }else if(cmd_type == CMD_TYPE_ASR) {
        processASRCmd(cmd_id);
    }


    return 0;
}

int ASRCOmmandProc::sendCmd(uint8_t cmd_type, uint16_t cmd_id)
{
    if(m_bUartInit==0) return ESP_FAIL;
    
    char buff[6];
    buff[0] = 0xA5;
    buff[1] = 0xFA;
    buff[2] = cmd_type;
    buff[3] = (uint8_t)(cmd_id >> 8);
    buff[4] = (uint8_t)(cmd_id & 0xFF);
    buff[5] = 0xFB;
    
    int n = uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) buff, sizeof(buff));
    if(n != sizeof(buff)) {
        ESP_LOGE(TAG, "Failed to send command %u:%u to ASR device.", cmd_type, cmd_id);
    }
    return 0;
}


int ASRCOmmandProc::processASRCmd(uint16_t cmd_id)
{
    PowerMonitor::getInstance()->updateInputActivityTime();

    SoundPlayer *pSndPlayer = SoundPlayer::getInstance();
    switch(cmd_id) {
        case 1:             //this is wakeup word. already handled in earlier steps
            return 0;
        case ASR_CMD_1SHOOT:
            EpicMgr::getInstance()->sendCmd(EPIC_CMD_SWITCH_EPIC, EPIC_SINGLESHOT);
            pSndPlayer->playSpeach(VOICE_1SHOOT_START); 
            break;
        case ASR_CMD_2SHOOT:
            EpicMgr::getInstance()->sendCmd(EPIC_CMD_SWITCH_EPIC, EPIC_DOUBLESHOT);
            pSndPlayer->playSpeach(VOICE_2SHOOT_START); 
            break;
        case ASR_CMD_8SHOOT:
            EpicMgr::getInstance()->sendCmd(EPIC_CMD_SWITCH_EPIC, EPIC_EIGHTSHOT);
            break;
        case ASR_CMD_TMLIMIT_SHOOT:
            EpicMgr::getInstance()->sendCmd(EPIC_CMD_SWITCH_EPIC, EPIC_TMLIMIT);
            break;
        case ASR_CMD_RANDOM_SHOOT:
            EpicMgr::getInstance()->sendCmd(EPIC_CMD_SWITCH_EPIC, EPIC_RANDOMSHOOT);
            break;
        case ASR_CMD_ATTACK_SHOOT:
            EpicMgr::getInstance()->sendCmd(EPIC_CMD_SWITCH_EPIC, EPIC_ADVATTACK);
            break;
        case ASR_CMD_MAX_VOLUME:
            pSndPlayer->setVolume(SOUND_VOLUME_MAX);
            pSndPlayer->playSpeach(VOICE_MAX_VOLUME); 
            break;
        case ASR_CMD_MIN_VOLUME:
            pSndPlayer->setVolume(SOUND_VOLUME_MIN);
            pSndPlayer->playSpeach(VOICE_MIN_VOLUME); 
            break;
        case ASR_CMD_MID_VOLUME:
            pSndPlayer->setVolume(SOUND_VOLUME_MID);
            pSndPlayer->playSpeach(VOICE_MID_VOLUME); 
            break;
        case ASR_CMD_VOICE_HELP:
            pSndPlayer->playSpeach(VOICE_HELP); 
            break;
        case ASR_CMD_REPORT_POWER:
            {
            int pct = PowerMonitor::getInstance()->getPower();
            SegDisplay::getInstance()->blinkNum(pct);
            pSndPlayer->playSpeachWithNum(VOICE_POWER_REPORT, pct);
            }
            break;
        case ASR_CMD_AIM_HELP:
            BLEMgr::getInstance()->setAimHelp();
            break;
    }
    //sendCmd(CMD_TYPE_CTL, CTL_CMD_DN_ASR);
    //char name[64];
    //sprintf(name, "asr/%u", cmd_id-1);
    //SoundPlayer::getInstance()->play((char*)name);
    //sendCmd(CMD_TYPE_CTL, CTL_CMD_EN_ASR);
    return 0;
}

