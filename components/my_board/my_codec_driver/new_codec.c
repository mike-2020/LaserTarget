#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "board.h"

#include "new_codec.h"

static const char *TAG = "new_codec";

static bool codec_init_flag;

audio_hal_func_t AUDIO_NEW_CODEC_DEFAULT_HANDLE = {
    .audio_codec_initialize = new_codec_init,
    .audio_codec_deinitialize = new_codec_deinit,
    .audio_codec_ctrl = new_codec_ctrl_state,
    .audio_codec_config_iface = new_codec_config_i2s,
    .audio_codec_set_mute = new_codec_set_voice_mute,
    .audio_codec_set_volume = new_codec_set_voice_volume,
    .audio_codec_get_volume = new_codec_get_voice_volume,
};

bool new_codec_initialized()
{
    return codec_init_flag;
}

esp_err_t new_codec_init(audio_hal_codec_config_t *cfg)
{
    ESP_LOGI(TAG, "new_codec init");
    return ESP_OK;
}

esp_err_t new_codec_deinit(void)
{
    return ESP_OK;
}

esp_err_t new_codec_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    return ESP_OK;
}

esp_err_t new_codec_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    return ESP_OK;
}

esp_err_t new_codec_set_voice_mute(bool mute)
{
    return ESP_OK;
}

esp_err_t new_codec_set_voice_volume(int volume)
{
    return ESP_OK;
}

esp_err_t new_codec_get_voice_volume(int *volume)
{
    return ESP_OK;
}
