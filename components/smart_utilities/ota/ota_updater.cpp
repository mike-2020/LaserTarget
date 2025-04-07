#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "http_cmd.h"
#include "smart_utility.h"

#define OTA_READ_BUFF_LEN   1024
const char *TAG = "OTA_UPDATER";
static TickType_t lastUpdateCheck = 0;
extern "C" void ota_check_and_update(const char *name)
{
    esp_err_t rc = ESP_OK;
    uint8_t ver[16]={0};
    uint8_t *pBuff = NULL;
    HTTPCmd *pHttp = NULL;
    int len = 0;
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *toUpdatePart = NULL;
    const esp_app_desc_t *pApp = NULL;
    char url[128];
    rc = read_local_server_url(url, sizeof(url));
    if(rc!=ESP_OK || strlen(url)==0) return;

    //only check image updates every 5min
    TickType_t tt = xTaskGetTickCount() - lastUpdateCheck;
    if(lastUpdateCheck != 0 && tt < 5 * 60 * 1000/portTICK_PERIOD_MS)
    {
        return;
    }
    lastUpdateCheck = xTaskGetTickCount();

    pHttp = new HTTPCmd(name, url);
    rc= pHttp->getNewImageBuildVersion(name, ver, sizeof(ver));
    if(rc!=0) {
        UDP_LOGW(TAG, "Failed to read image version on remote server.(%d)", rc);
        goto EXIT_ENTRY;
    }
    //const char *ver = "123";

    pApp = esp_app_get_description();
    if(ver[0]=='\0' || strcmp((const char*)ver, pApp->version)<=0){
        ESP_LOGI(TAG, "Remote image version %s is the same with current app.", ver);
        goto EXIT_ENTRY;
    }

    UDP_LOGI(TAG, "Remote image version: %s, current app version: %s.", ver, pApp->version);
    UDP_LOGI(TAG, "Start OTA update process...");
    sprintf(url, "/upgrade/download?name=%s", name);
    rc = pHttp->open(url, 0);
    if(rc<0) {
        UDP_LOGI(TAG, "Failed to connect to server for upgrade: %s.", url);
        goto EXIT_ENTRY;
    }

    pBuff = (uint8_t*)malloc(OTA_READ_BUFF_LEN);
    if(pBuff==NULL) {
        ESP_LOGE(TAG, "Failed to allocate memroy for  ota_check_and_update.");
        goto EXIT_ENTRY;
    }

    toUpdatePart = esp_ota_get_next_update_partition(NULL);
    if(toUpdatePart==NULL) {
        UDP_LOGI(TAG, "Failed to find next OTA partition.");
        goto EXIT_ENTRY;
    }

    rc = esp_ota_begin(toUpdatePart, OTA_SIZE_UNKNOWN, &ota_handle);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to call esp_ota_begin, rc=%d.", rc);
        goto EXIT_ENTRY;
    }

    while(1)
    {
        len = pHttp->read(pBuff, OTA_READ_BUFF_LEN);
        if(len<=0) break;
        rc = esp_ota_write(ota_handle, pBuff, len);
        if(rc!=ESP_OK) {
            ESP_LOGE(TAG, "Failed to call esp_ota_write, rc=%d.", rc);
            break;
        }
    }
    pHttp->close();

    if(len<0) {
        ESP_LOGE(TAG, "OTA failed to read remote image, rc=%d.", len);
        goto EXIT_ENTRY;
    }
    if(rc!=ESP_OK) {
        goto EXIT_ENTRY;
    }

    rc = esp_ota_end(ota_handle);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to call esp_ota_end, rc=%d.", rc);
        goto EXIT_ENTRY;
    }

    esp_ota_set_boot_partition(toUpdatePart);

    ESP_LOGE(TAG, "OTA upgrade finished, restarting...");
    UDP_LOGI(TAG, "==================================================");
    UDP_LOGI(TAG, "OTA upgrade finished, restarting...");
    UDP_LOGI(TAG, "==================================================");
    vTaskDelay(1000 / portTICK_PERIOD_MS); //wait for 1 seconds to finish sendig last log
    //HTTPCmd::play("~/mp3/finish_upgrade.mp3");
    esp_restart();

EXIT_ENTRY:
    if(pHttp!=NULL) delete pHttp;
    if(pBuff!=NULL) free(pBuff);
}
