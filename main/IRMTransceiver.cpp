#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "IRMNECEncoder.h"
#include "IRMTransceiver.h"



#define EXAMPLE_IR_RESOLUTION_HZ     1000000 // 1MHz resolution, 1 tick = 1us
#define EXAMPLE_IR_TX_GPIO_NUM       GPIO_NUM_9

static const char *TAG = "IRM_TRANSCEIVER";
static rmt_transmit_config_t transmit_config;

int IRMTransceiver::init()
{
    ESP_LOGI(TAG, "install IR NEC encoder");
    ir_nec_encoder_config_t nec_encoder_cfg = {
        .resolution = EXAMPLE_IR_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_ir_nec_encoder(&nec_encoder_cfg, &m_NecEncoder));

    // this example won't send NEC frames in a loop
    transmit_config = {
        .loop_count = 0, // no loop
    };

    return prepare();
}

int IRMTransceiver::prepare()
{
    ESP_LOGI(TAG, "create RMT TX channel");
    rmt_tx_channel_config_t tx_channel_cfg = {
        .gpio_num = EXAMPLE_IR_TX_GPIO_NUM,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = EXAMPLE_IR_RESOLUTION_HZ,
        .mem_block_symbols = 48, // amount of RMT symbols that the channel can store at a time
        .trans_queue_depth = 4,  // number of transactions that allowed to pending in the background, this example won't queue multiple transactions, so queue depth > 1 is sufficient
    };
    
    //tx_channel_cfg.flags.with_dma = true;

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &m_TxChannel));

    ESP_LOGI(TAG, "modulate carrier to TX channel");
    rmt_carrier_config_t carrier_cfg = {
        .frequency_hz = 38000, // 38KHz
        .duty_cycle = 0.33,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(m_TxChannel, &carrier_cfg));

    ESP_LOGI(TAG, "enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(m_TxChannel));
    return 0;
}

int IRMTransceiver::send(uint8_t addr, uint8_t cmd)
{
    ir_nec_scan_code_t scan_code = {
        .address = (uint16_t) (((uint16_t)addr << 8) | ((~addr) & 0xFF)), //0x0440,
        .command = (uint16_t) (((uint16_t)cmd << 8) | ((~cmd) & 0xFF)), //0x3003,
    };
    ESP_LOGI(TAG, "addr = 0x%X, cmd = 0x%X.", scan_code.address, scan_code.command);
    
    ESP_ERROR_CHECK(rmt_transmit(m_TxChannel, m_NecEncoder, &scan_code, sizeof(scan_code), &transmit_config));
    return 0;
}