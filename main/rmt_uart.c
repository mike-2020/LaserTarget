#include <string.h>
#include "rmt_uart.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/rmt_tx.h"
#include "hal/rmt_types.h"
#include "driver/uart.h"
#include "driver/gpio.h"


static const char *TAG = "rmt-uart";

typedef struct {
    rmt_symbol_word_t * items;
    int item_index;
} rmt_uart_contex_tx_t;

typedef struct {
    uint8_t* bytes;
    int byte_num;
    int bit_num;
    uint16_t raw_data;
    RingbufHandle_t rb;
} rmt_uart_contex_rx_t;

typedef struct {
    //rmt_tx_channel_config_t rmt_config_tx;
    //rmt_config_t rmt_config_rx;
    rmt_uart_config_t rmt_uart_config;
    rmt_uart_contex_tx_t rmt_uart_contex_tx;
    //rmt_uart_contex_rx_t rmt_uart_contex_rx;
    uint16_t rmt_bit_len;
    bool configured;
} rmt_uart_contex_t;

static rmt_uart_contex_t rmt_uart_contex[RMT_UART_NUM_MAX] = {0};

static rmt_encoder_handle_t rfEncoder;
static rmt_channel_handle_t txChannel;

static int convert_byte_to_items(rmt_uart_contex_t* ctx, uint8_t byte)
{
    rmt_uart_contex_tx_t* rtc = &ctx->rmt_uart_contex_tx;
    uint16_t data = (byte << 1) | (3 << 10); //[1 start bit][8 data bit][1 parity bit][1 stop bit][1 idle bit]
    int n = 0;
    for(int i = 0; i<8; i++)
    {
        if(((byte >> i) & 0x1) == 0x1) n++;
    }
    if(n%2==0) data = data | 0x0200;  //set parity bit

    //ESP_LOGI(TAG, "byte = 0x%x, data = 0x%x", byte, data);

    for (int i = 0; i < 12; i += 2)
    {
        rmt_symbol_word_t * item = &rtc->items[rtc->item_index];
        item->duration0 = ctx->rmt_bit_len;
        item->duration1 = ctx->rmt_bit_len;
        item->level0 = (data >> i) ^ 1;             //this is because "flags.invert_out = true"
        item->level1 = (data >> (i + 1)) ^ 1;       //this is because "flags.invert_out = true"
        rtc->item_index++;
        if (rtc->item_index >= (ctx->rmt_uart_config.buffer_size / sizeof(rmt_symbol_word_t)))
        {
            ESP_LOGE(TAG, "DATA TOO LONG");
            return -1;
        }
        //ESP_LOGI(TAG, "\trmt tx item %02d: duration0: %d level0: %d  duration1: %d level1: %d", rtc->item_index, item->duration0, item->level0, item->duration1, item->level1);
    }
    return 0;
}


static int convert_data_to_items(rmt_uart_contex_t* ctx, const uint8_t* data, uint16_t len)
{
    rmt_uart_contex_tx_t* rtc = &ctx->rmt_uart_contex_tx;
    rtc->item_index = 0;
    
    for (int i = 0; i < len; ++i)
    {
        if (convert_byte_to_items(ctx, data[i])) return -1;
        //ESP_LOGI(TAG, "rmt tx item");
    }
    return rtc->item_index;
}

/* 初始化编码器 */
void init_encoder()
{
    // 目前配置不需要参数
    rmt_copy_encoder_config_t cfg = {};
    // 创建拷贝编码器
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&cfg, &rfEncoder));
}



esp_err_t rmt_uart_init(rmt_uart_port_t uart_num, const rmt_uart_config_t* uart_config)
{
    ESP_RETURN_ON_FALSE((uart_num < RMT_UART_NUM_MAX), ESP_FAIL, TAG, "uart_num error");
    ESP_RETURN_ON_FALSE((uart_config), ESP_FAIL, TAG, "uart_config error");

    const int RMT_HZ = 2 * 1000 * 1000;
    uint16_t bit_len = RMT_HZ / uart_config->baud_rate;
    ESP_LOGI(TAG, "baud=%d, bit_len=%d, uart_config->tx_io_num=%d", uart_config->baud_rate, bit_len, uart_config->tx_io_num);
    ESP_RETURN_ON_FALSE(((10UL * bit_len) < 0xFFFF), ESP_FAIL, TAG, "rmt tick too long, reconfigure 'resolution_hz'");
    ESP_RETURN_ON_FALSE(((bit_len) > 49), ESP_FAIL, TAG, "rmt tick too long, reconfigure 'resolution_hz'");
    ESP_RETURN_ON_FALSE(((bit_len / 2) < 0xFF), ESP_FAIL, TAG, "baud rate too slow, reconfigure 'resolution_hz'");

    rmt_uart_contex[uart_num].rmt_bit_len = bit_len;
    memcpy(&rmt_uart_contex[uart_num].rmt_uart_config, uart_config, sizeof(rmt_uart_config_t));

#if CONFIG_SPIRAM_USE_MALLOC
        rmt_uart_contex[uart_num].rmt_uart_contex_tx.items = heap_caps_calloc(1, uart_config->buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
        rmt_uart_contex[uart_num].rmt_uart_contex_tx.items = calloc(1, uart_config->buffer_size);
#endif

    //rmt_channel_handle_t tx_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,   // select source clock
        .gpio_num = uart_config->tx_io_num,                    // GPIO number
        .mem_block_symbols = 48,          // memory block size, 64 * 4 = 256 Bytes
        .resolution_hz = RMT_HZ,          //2 * 1000 * 1000 tick resolution, i.e., 1 tick = 0.5 µs
        .trans_queue_depth = 4,           // set the number of transactions that can pend in the background
        .flags.invert_out = true,         // invert output signal, follow UART rule
        .flags.with_dma = false,          // do not need DMA backend
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &txChannel));
    ESP_ERROR_CHECK(rmt_enable(txChannel));

    rmt_uart_contex[uart_num].configured = true;

    init_encoder();
    return 0;
}



esp_err_t rmt_uart_write_bytes(rmt_uart_port_t uart_num, const uint8_t* data, size_t size)
{
    rmt_uart_contex_t* ctx = &rmt_uart_contex[uart_num];
    ESP_RETURN_ON_FALSE((ctx->configured), ESP_FAIL, TAG, "uart not configured");
    ESP_RETURN_ON_FALSE((ctx->rmt_uart_config.mode != RMT_UART_MODE_RX_ONLY), ESP_FAIL, TAG, "uart RX only");
    rmt_uart_contex_tx_t* rtc = &ctx->rmt_uart_contex_tx;

    if (convert_data_to_items(ctx, data, size) < 0) return ESP_FAIL;

    rmt_transmit_config_t transmit_config = {
        .loop_count = 0, // no loop
    };

    if(txChannel==NULL) {
        ESP_LOGE(TAG, "invalid txChannel.");
        return ESP_FAIL;
    }
    
    ESP_ERROR_CHECK(rmt_transmit(txChannel, rfEncoder, rtc->items, rtc->item_index*sizeof(rmt_symbol_word_t), &transmit_config));
    //ESP_ERROR_CHECK(rmt_disable(txChannel));
    return 0;
}

