#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "smart_utility.h"

#define DEFAULT_LISTEN_INTERVAL 3
#define DEFAULT_BEACON_TIMEOUT  6

#if CONFIG_EXAMPLE_POWER_SAVE_MIN_MODEM
#define DEFAULT_PS_MODE WIFI_PS_MIN_MODEM
#elif CONFIG_EXAMPLE_POWER_SAVE_MAX_MODEM
#define DEFAULT_PS_MODE WIFI_PS_MAX_MODEM
#elif CONFIG_EXAMPLE_POWER_SAVE_NONE
#define DEFAULT_PS_MODE WIFI_PS_NONE
#else
#define DEFAULT_PS_MODE WIFI_PS_NONE
#endif /*CONFIG_POWER_SAVE_MODEM*/
#if CONFIG_IDF_TARGET_ESP32C2
#define CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ 80
#define CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ 26
#else
#define CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ 80
#define CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ 40
#endif

static const char *TAG = "POWER_SAVE_C";

int enable_power_save_mode(int mode)
{
#if CONFIG_PM_ENABLE
    // Configure dynamic frequency scaling:
    // maximum and minimum frequencies are set in sdkconfig,
    // automatic light sleep is enabled if tickless idle support is enabled.
#if CONFIG_IDF_TARGET_ESP32
    esp_pm_config_esp32_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32S2
    esp_pm_config_esp32s2_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32C3
    esp_pm_config_esp32c3_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32S3
    esp_pm_config_esp32s3_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32C2
    esp_pm_config_esp32c2_t pm_config = {
#endif
            .max_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
            .min_freq_mhz = CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
            .light_sleep_enable = true
#endif
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
    ESP_LOGW(TAG, "Enable power management. (light_sleep_enable=%d)", pm_config.light_sleep_enable);
#endif // CONFIG_PM_ENABLE


    ESP_ERROR_CHECK(esp_wifi_set_inactive_time(WIFI_IF_STA, DEFAULT_BEACON_TIMEOUT));

    ESP_LOGI(TAG, "esp_wifi_set_ps().");
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

    return ESP_OK;
}


int enter_deep_sleep(int deep_sleep_sec)
{
    UDP_LOGI(TAG, "Device is running into deep sleep mode(%ds)...", deep_sleep_sec);
    ESP_LOGW(TAG, "***************************************************");
    ESP_LOGW(TAG, "Entering deep sleep for %d seconds", deep_sleep_sec);
    ESP_LOGW(TAG, "***************************************************");
    esp_wifi_stop();
    esp_sleep_enable_timer_wakeup(1000000LL * deep_sleep_sec);
    esp_deep_sleep_start();
}

