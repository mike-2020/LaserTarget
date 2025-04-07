#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "hal/pcnt_types.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "esp_clk_tree.h"
#include "LaserReceiver_PCNT.h"


#define EXAMPLE_PCNT_HIGH_LIMIT 10000
#define EXAMPLE_PCNT_LOW_LIMIT  -10000
#define PCNT_GPIO_NUM   GPIO_NUM_40

pcnt_unit_handle_t pcnt_unit = NULL;
int pcnt_last_count = 0;
static const char *TAG = "LASER_RECEIVER";

#define PCNT_FREQ_MIN   1000  //Hz
#define TIMER_DIVIDER         (16)  //  Hardware timer clock divider


int LR_PCNT::init()
{
    #if 0
    ESP_LOGI(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .high_limit = EXAMPLE_PCNT_HIGH_LIMIT,
        .low_limit = EXAMPLE_PCNT_LOW_LIMIT,
    };
    
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    ESP_LOGI(TAG, "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    ESP_LOGI(TAG, "install pcnt channels");
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = PCNT_GPIO_NUM,
        .level_gpio_num = -1,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    ESP_LOGI(TAG, "set edge and level actions for pcnt channels");
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_HOLD, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    //ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_HOLD));

    ESP_LOGI(TAG, "add watch points and register callbacks");
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, 100));

    pcnt_event_callbacks_t cbs = {
        .on_reach = example_pcnt_on_reach,
    };
    
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, queue));

    ESP_LOGI(TAG, "enable pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_LOGI(TAG, "clear pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_LOGI(TAG, "start pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    uint32_t apb_freq = 0;
    esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_APB, ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT, &apb_freq);
    ESP_LOGW(TAG, "apb_freq = %luHz", apb_freq);
    pcnt_freq_check_timer((apb_freq/TIMER_DIVIDER/1000), queue);  //1ms

    #endif
    return ESP_OK;
}
