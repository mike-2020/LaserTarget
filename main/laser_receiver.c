#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "hal/pcnt_types.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "esp_clk_tree.h"

#define EXAMPLE_PCNT_HIGH_LIMIT 10000
#define EXAMPLE_PCNT_LOW_LIMIT  -10000
#define PCNT_GPIO_NUM   GPIO_NUM_40

pcnt_unit_handle_t pcnt_unit = NULL;
int pcnt_last_count = 0;
static const char *TAG = "LASER_RECEIVER";

#define PCNT_FREQ_MIN   1000  //Hz
#define TIMER_DIVIDER         (16)  //  Hardware timer clock divider
//#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

//*****************************************
//*****************************************
//********** TIMER TG0 INTERRUPT **********
//*****************************************
//*****************************************
uint16_t dismiss_counter = 0;
#ifdef USE_GPTIMER
static bool IRAM_ATTR  cbs_pcnt_freq_check_timer(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
#else

static bool IRAM_ATTR cbs_pcnt_freq_check_timer(void *args)
#endif
{
    BaseType_t high_task_wakeup = pdFALSE;
	//Reset irq and set for next time
    //TIMERG0.int_clr_timers.t0 = 1;
    //TIMERG0.hw_timer[0].config.alarm_en = 1;

    int count = 0;
    uint32_t freq = 0;
    if(dismiss_counter>0) dismiss_counter--;

    pcnt_unit_get_count(pcnt_unit, &count);
    freq = count * 1000;
    if(freq > 35*1000 && dismiss_counter == 0) {
        QueueHandle_t queue = (QueueHandle_t)args;
        xQueueSendFromISR(queue, &(freq), &high_task_wakeup);
        dismiss_counter = 500;  //no more than one trigger per 500ms
    }

    pcnt_unit_clear_count(pcnt_unit);  //reset pcnt counter to zero. it means a new measure is started
    return (high_task_wakeup == pdTRUE);
}


//******************************************
//******************************************
//********** TIMER TG0 INITIALISE **********
//******************************************
//******************************************
#if USE_GPTIMER
void pcnt_freq_check_timer (int timer_period_us)
{
    gptimer_handle_t gptimer = NULL;

    gptimer_config_t config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
    };

    ESP_ERROR_CHECK(gptimer_new_timer(&config, &gptimer));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0, // counter will reload with 0 on alarm event
        .alarm_count = 1000, // period = 1ms @resolution 1MHz
        .flags.auto_reload_on_alarm = true, // enable auto-reload
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = cbs_pcnt_freq_check_timer, // register user callback
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
}
#endif 
void pcnt_freq_check_timer (int timer_period_us, QueueHandle_t queue)
{
    int group = TIMER_GROUP_0;
    int timer = TIMER_0;
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true,
    }; // default clock source is APB
    timer_init(group, timer, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(group, timer, 0);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(group, timer, timer_period_us);
    timer_enable_intr(group, timer);
    timer_isr_callback_add(group, timer, cbs_pcnt_freq_check_timer, queue, 0);

    timer_start(group, timer);
}


static bool example_pcnt_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx)
{
    //BaseType_t high_task_wakeup;
    //QueueHandle_t queue = (QueueHandle_t)user_ctx;
    // send event data to queue, from this interrupt callback
    //xQueueSendFromISR(queue, &(edata->watch_point_value), &high_task_wakeup);

    //pcnt_unit_clear_count(unit);
    
    return pdFALSE;
}

void laser_receiver_start(QueueHandle_t queue)
{
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

}


