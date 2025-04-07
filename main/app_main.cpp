/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "LedStrip.h"
#include "SoundPlayer.h"
#include "TargetServo.h"
#include "SegDisplay.h"
#include "ASRCOmmandProc.h"
#include "RadarMgr.h"
#include "LaserReceiver_IS0803.h"
#include "EpicMgr.h"
#include "PowerMonitor.h"
#include "BLEMgr.h"
#include "AutoTurret.h"
#include "IRMTransceiver.h"


static const char *TAG = "main";

extern "C" void laser_receiver_start(QueueHandle_t queue);

static esp_err_t esp_storage_init(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    return ret;
}


extern "C" void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_storage_init();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    //install gpio isr service
    gpio_install_isr_service(0);

    BLEMgr::getInstance()->start();

    //gpio_set_direction(GPIO_NUM_9, GPIO_MODE_OUTPUT);
    //gpio_set_level(GPIO_NUM_9, 0);
    //vTaskDelay(10000 / portTICK_PERIOD_MS);
    //IRMTransceiver irm;
    //irm.init();
    //while(1) {
    //    irm.send(0x11, 0x22);
    //    vTaskDelay(1000 / portTICK_PERIOD_MS);
    //}
#if 1
    SoundPlayer *sndPlayer = SoundPlayer::getInstance();
    sndPlayer->init();
    sndPlayer->playSpeach(VOICE_WELCOME);

    PowerMonitor *powerMonitor = PowerMonitor::getInstance();
    powerMonitor->start();
    powerMonitor->powerOn5V();  //Enable VCC_5V

    LR_IS0803 *laserRecv = LR_IS0803::getInstance();
    laserRecv->init();

    LEDStrip *ledStrip = LEDStrip::getInstance();
    ledStrip->init();

    TargetServo *tgtServo = TargetServo::getInstance();
    tgtServo->init();

    SegDisplay *segDisplay = SegDisplay::getInstance();
    segDisplay->init();
    segDisplay->displayNum(99);

    RadarMgr *radar = RadarMgr::getInstance();
    radar->init();

    AutoTurret *autoTurret = AutoTurret::getInstance();
    autoTurret->init();

    ASRCOmmandProc asrCmd = ASRCOmmandProc();
    asrCmd.init();

   
    EpicMgr *epicMgr = EpicMgr::getInstance();
    epicMgr->run();

#endif 
}


