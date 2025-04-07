#include "esp_log.h"
#include "board.h"
#include "audio_mem.h"
#include "audio_hal.h"
#include "periph_sdcard.h"

static const char *TAG = "AUDIO_BOARD";

static audio_board_handle_t board_handle = 0;

extern audio_hal_func_t AUDIO_CODEC_ES7148_DEFAULT_HANDLE;

audio_board_handle_t audio_board_init(void)
{
    if (board_handle) {
        ESP_LOGW(TAG, "The board has already been initialized!");
        return board_handle;
    }
    board_handle = (audio_board_handle_t) audio_calloc(1, sizeof(struct audio_board_handle));
    AUDIO_MEM_CHECK(TAG, board_handle, return NULL);
    board_handle->audio_hal = audio_board_codec_init();

    return board_handle;
}

audio_hal_handle_t audio_board_codec_init(void)
{
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_hal_handle_t codec_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES7148_DEFAULT_HANDLE);
    AUDIO_NULL_CHECK(TAG, codec_hal, return NULL);
    return codec_hal;
}


esp_err_t audio_board_sdcard_init(esp_periph_set_handle_t set, periph_sdcard_mode_t mode)
{
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = get_sdcard_intr_gpio(), // GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    esp_err_t ret = esp_periph_start(set, sdcard_handle);
    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    return ret;
}

audio_board_handle_t audio_board_get_handle(void)
{
    return board_handle;
}

esp_err_t audio_board_deinit(audio_board_handle_t audio_board)
{
    esp_err_t ret = ESP_OK;
    ret |= audio_hal_deinit(audio_board->audio_hal);
    free(audio_board);
    board_handle = NULL;
    return ret;
}
