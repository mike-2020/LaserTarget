#include "nvs_flash.h"
#include "esp_log.h"
#include "smart_utility.h"

#define NVS_NS_WIFI "app_wifi"
#define NVS_NS_APP_SERVER  "app_server"


static const char *TAG = "NVS_DB_C";

int save_local_server_url(const char *url)
{
    int rc = ESP_OK;
    nvs_handle_t handle;
    rc = nvs_open(NVS_NS_APP_SERVER, NVS_READWRITE, &handle);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to open namespace %s from NVS. (%d)", NVS_NS_APP_SERVER, rc);
        return ESP_FAIL;
    }
    rc = nvs_set_str(handle, "local.url", url);
    if(rc!=ESP_OK){
        ESP_LOGE(TAG, "Failed to save local.url into NVS. (%d)", rc);
        nvs_close(handle);
        return ESP_FAIL;
    }
    rc = nvs_commit(handle);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit namespace %s from NVS. (%d)", NVS_NS_APP_SERVER, rc);
        nvs_close(handle);
        return ESP_FAIL;
    }
    nvs_close(handle);
    return ESP_OK;
}

int read_local_server_url(char *url, size_t len)
{
    int rc = ESP_OK;
    nvs_handle_t handle;
    rc = nvs_open(NVS_NS_APP_SERVER, NVS_READONLY, &handle);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to open namespace %s from NVS. (%d)", NVS_NS_APP_SERVER, rc);
        return ESP_FAIL;
    }

    rc = nvs_get_str(handle, "local.url", url, &len);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to read local.url from NVS. (%d)", rc);
        nvs_close(handle);
        return ESP_FAIL;
    }

    nvs_close(handle);
    return ESP_OK;
}

int read_wifi_cred(char *ssid, char *password)
{
    int rc = ESP_OK;
    nvs_handle_t handle;
    rc = nvs_open(NVS_NS_WIFI, NVS_READONLY, &handle);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to open namespace %s from NVS. (%d)", NVS_NS_WIFI, rc);
        return ESP_FAIL;
    }

    size_t len = WIFI_CRED_LEN;
    rc = nvs_get_str(handle, "wifi.ssid", ssid, &len);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to read wifi.ssid from NVS. (%d)", rc);
        nvs_close(handle);
        return ESP_FAIL;
    }
    len = WIFI_CRED_LEN;
    rc = nvs_get_str(handle, "wifi.password", password, &len);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to read wifi.password from NVS. (%d)", rc);
        nvs_close(handle);
        return ESP_FAIL;
    }
    nvs_close(handle);
    return ESP_OK;
}

int save_wifi_cred(const char *ssid, const char *password)
{
    int rc = ESP_OK;
    nvs_handle_t handle;
    rc = nvs_open(NVS_NS_WIFI, NVS_READWRITE, &handle);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to open namespace %s from NVS. (%d)", NVS_NS_WIFI, rc);
        return ESP_FAIL;
    }
    rc = nvs_set_str(handle, "wifi.ssid", ssid);
    rc = nvs_set_str(handle, "wifi.password", password);
    rc = nvs_commit(handle);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit namespace %s from NVS. (%d)", NVS_NS_WIFI, rc);
        nvs_close(handle);
        return ESP_FAIL;
    }
    nvs_close(handle);
    return ESP_OK;
}

