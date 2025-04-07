#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "SegDisplay.h"
#include "rmt_uart.h"

#define SEGDSP_TEST_TXD (GPIO_NUM_7)
#define SEGDSP_UART_PORT_NUM      (0)
#define SEGDSP_UART_BAUD_RATE     (19200)
#define SEGDSP_TASK_STACK_SIZE    1024*3

#define SEGDSP_CMD_DISPLAY      1
#define SEGDSP_CMD_BLINK        2

static uint8_t g_digits_map[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
static const char *TAG = "SEG_DISPLAY";

static SegDisplay g_segDisplay;

int SegDisplay::init()
{
    int rc = 0;
    rmt_uart_config_t uart_config = {
        .baud_rate      = SEGDSP_UART_BAUD_RATE,                        /*!< UART baud rate*/
        .mode           = RMT_UART_MODE_TX_ONLY,        /*!< UART mode*/  
        .data_bits      = RMT_UART_DATA_8_BITS,         /*!< UART byte size*/
        .parity         = RMT_UART_PARITY_DISABLE,      /*!< UART parity mode*/
        .stop_bits      = RMT_UART_STOP_BITS_1,         /*!< UART stop bits*/
        .tx_io_num      = SEGDSP_TEST_TXD,              /*!< UART TX GPIO num*/
        .rx_io_num      = (gpio_num_t)-1,                           /*!< UART RX GPIO num*/
        .buffer_size    = 256,                          /*!< UART buffer size>*/
    };

    rc = rmt_uart_init(0, &uart_config);
    if(rc<0) return rc;

    #if 0
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = SEGDSP_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_ODD,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    //ESP_ERROR_CHECK(uart_driver_install(SEGDSP_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    //ESP_ERROR_CHECK(uart_param_config(SEGDSP_UART_PORT_NUM, &uart_config));
    //ESP_ERROR_CHECK(uart_set_pin(SEGDSP_UART_PORT_NUM, SEGDSP_TEST_TXD, SEGDSP_TEST_RXD, SEGDSP_TEST_RTS, SEGDSP_TEST_CTS));
    #endif 
    m_cmdQueue = xQueueCreate(2, sizeof(seg_display_cmd_t));
    xTaskCreate(SegDisplay::proc, "display_num_task", SEGDSP_TASK_STACK_SIZE, (void*)this, 10, NULL);


    m_buff[0] = 0x08;   //display number command
    m_buff[1] = 0x00;
    m_buff[2] = 0x00;
    m_buff[3] = 0x00;
    m_buff[4] = 0x00;
    m_buff[5] = 0x00;
    m_buff[6] = 0x18;   //display control command
    m_buff[7] = 0x4F;

    ESP_LOGI(TAG, "SegDisplay has been initialized.");
    return 0;
}

void SegDisplay::clear()
{
    m_buff[1] = 0x00;
    m_buff[2] = 0x00;
    rmt_uart_write_bytes(SEGDSP_UART_PORT_NUM, (const uint8_t *) m_buff, 6);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    rmt_uart_write_bytes(SEGDSP_UART_PORT_NUM, (const uint8_t *) &m_buff[6], 2);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void SegDisplay::proc(void *ctx)
{
    SegDisplay *handle = (SegDisplay*)ctx;
    seg_display_cmd_t cmd;

    while(1) {
        if (xQueueReceive(handle->m_cmdQueue, &cmd, pdMS_TO_TICKS(1000))!=pdTRUE) continue;
        switch(cmd.cmd) {
            case SEGDSP_CMD_DISPLAY:
                handle->_displayNum(cmd.val);
                break;
            case SEGDSP_CMD_BLINK:
                handle->_blinkNum(cmd.val);
                break;
        }
        
    }
}

void SegDisplay::_displayNum(uint16_t num)
{
    int digit = num % 10;
    m_buff[2] = g_digits_map[digit];
    num = num / 10;
    if(num > 0) {
        digit = num % 10;
        m_buff[1] = g_digits_map[digit];
    }else{
        m_buff[1] = 0;
    }

    rmt_uart_write_bytes(SEGDSP_UART_PORT_NUM, (const uint8_t *) m_buff, 6);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    rmt_uart_write_bytes(SEGDSP_UART_PORT_NUM, (const uint8_t *) &m_buff[6], 2);
    vTaskDelay(10 / portTICK_PERIOD_MS);

}

void SegDisplay::_blinkNum(uint16_t num)
{
    for(int i=0; i<5; i++) {
        _displayNum(num);
        vTaskDelay(300 / portTICK_PERIOD_MS);
        clear();
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
    _displayNum(num);
}

int SegDisplay::displayNum(uint16_t num)
{
    seg_display_cmd_t cmd;
    cmd.cmd = SEGDSP_CMD_DISPLAY;
    cmd.val = num;
    BaseType_t rc = xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(10));
    if(rc!=pdTRUE) {
        ESP_LOGE(TAG, "Failed to send number display command: %u.", num);
        return ESP_FAIL;
    }
    return ESP_OK;
}

int SegDisplay::blinkNum(uint16_t num)
{
    seg_display_cmd_t cmd;
    cmd.cmd = SEGDSP_CMD_BLINK;
    cmd.val = num;
    BaseType_t rc = xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(10));
    if(rc!=pdTRUE) {
        ESP_LOGE(TAG, "Failed to send number blink command: %u.", num);
        return ESP_FAIL;
    }
    return ESP_OK;
}

SegDisplay* SegDisplay::getInstance()
{
    return &g_segDisplay;
}
