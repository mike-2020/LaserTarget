#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "RadarMgr.h"

#define ECHO_TEST_TXD (GPIO_NUM_45)
#define ECHO_TEST_RXD (GPIO_NUM_48)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)
#define RADARMGR_UART_PORT_NUM      ((uart_port_t)(2))
#define ECHO_UART_BAUD_RATE     (115200)
#define ECHO_TASK_STACK_SIZE    1024*3
#define GPIO_RADAR_OUT   (GPIO_NUM_19)
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_RADAR_OUT)


#define RD03_CMD_OPEN       0xFF
#define RD03_CMD_CLOSE      0xFE

#define UART_PACKET_LEN     18
static uint8_t cmd_tpl[] = { 0xFD, 0xFC, 0xFB, 0xFA, 0x0, 0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01};
static uint8_t cmd_recv[UART_PACKET_LEN];
#define CAL_CHECKCODE() (cmd_tpl[2] ^ cmd_tpl[3] ^ cmd_tpl[4])


static const char *TAG = "RADAR_MGR";
static RadarMgr g_radarMgr;

RadarMgr::RadarMgr()
{
    m_bUARTDeviceAcquired = false;
}

RadarMgr* RadarMgr::getInstance()
{
    return &g_radarMgr;
}

static int64_t last_trigger_time = 0;
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    RadarMgr *handle = (RadarMgr*) arg;
    

    uint32_t cmd = 1;
    int64_t tm = esp_timer_get_time();
    if(tm - last_trigger_time < 500*1000) {     //ignore new triggers in 0.5s
        //return;
    }
    last_trigger_time = tm;
    xQueueSendFromISR(handle->m_cmdQueue, &cmd, NULL);
}

static void radar_mgr_task(void *arg)
{
    RadarMgr *handle = (RadarMgr*) arg;
    uint32_t cmd = 1;
    while(1) {
        if (xQueueReceive(handle->m_cmdQueue, &cmd, pdMS_TO_TICKS(1000))!=pdTRUE) continue;
        ESP_LOGW(TAG, "Movement detected!!!");
    }
}

int RadarMgr::init()
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);  //Radar device needs time to initialize

    gpio_config_t io_conf = {};

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)1;
    gpio_config(&io_conf);

    //change gpio interrupt type for one pin
    //gpio_set_intr_type(GPIO_RADAR_OUT, GPIO_INTR_POSEDGE);

    //create a queue to handle gpio event from isr
    //m_cmdQueue = xQueueCreate(2, sizeof(uint32_t));
    //start gpio task
    //xTaskCreate(radar_mgr_task, "radar_mgr_task", 2048, (void*)this, 10, NULL);

    //hook isr handler for specific gpio pin
    //gpio_isr_handler_add(GPIO_RADAR_OUT, gpio_isr_handler, (void*) this);

    return 0;
}

int RadarMgr::acquireUARTDevice()
{
    if(m_bUARTDeviceAcquired==true) return 0;
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(RADARMGR_UART_PORT_NUM, UART_FIFO_LEN * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(RADARMGR_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(RADARMGR_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    m_bUARTDeviceAcquired = true;
    ESP_LOGI(TAG, "RadarMgr has acquired UART2 device.");
    return 0;
}

int RadarMgr::releaseUARTDevice()
{
    if(m_bUARTDeviceAcquired==true) {
        uart_driver_delete(RADARMGR_UART_PORT_NUM);
        m_bUARTDeviceAcquired = false;
        ESP_LOGI(TAG, "RadarMgr has released UART2 device.");
    }
    return 0;
}


int RadarMgr::buildOutgoingPacket(uint16_t cmd, uint16_t data1, uint16_t data2, uint16_t len)
{
    int i = 4;
    cmd_tpl[i++] = (2+len) & 0xFF;
    cmd_tpl[i++] = (2+len) >> 8;
    cmd_tpl[i++] = cmd & 0xFF;
    cmd_tpl[i++] = cmd >> 8;
    if(len>=2) {
        cmd_tpl[i++] = data1 & 0xFF;
        cmd_tpl[i++] = data1 >> 8;
    }
    if(len==6) {
        cmd_tpl[i++] = data2 & 0xFF;
        cmd_tpl[i++] = (data2 >> 8) & 0xFF;
        cmd_tpl[i++] = (data2 >> 16) & 0xFF;
        cmd_tpl[i++] = (data2 >> 24) & 0xFF;
    }
    cmd_tpl[i++] = 0x04;
    cmd_tpl[i++] = 0x03;
    cmd_tpl[i++] = 0x02;
    cmd_tpl[i++] = 0x01;
    return  i;
}

#define RADAR_CMD_OPEN()              buildOutgoingPacket(0xFF, 0x01, 0x00, 2)
#define RADAR_CMD_CLOSE()             buildOutgoingPacket(0xFE, 0x00, 0x00, 0)
#define RADAR_CMD_MIN_DETACT(X)     buildOutgoingPacket(0x07, 0x07, 0x00, (uint32_t)(X/0.7), 6)
#define RADAR_CMD_MAX_DETACT(X)     buildOutgoingPacket(0x07, 0x07, 0x01, (uint32_t)(X/0.7), 6)
#define RADAR_CMD_SET_TGT_DELAY(X)      buildOutgoingPacket(0x07, 0x04, (uint32_t)(X), 6)
#define RADAR_CMD_GET_TGT_DELAY()       buildOutgoingPacket(0x08, 0x04, 0x00, 2)
#define RADAR_GET_CMD()             (((uint16_t)cmd_tpl[7] << 8) | cmd_tpl[6])
#define RADAR_GET_LEN(X)            (((uint16_t)X[5] << 8) | X[4])

int RadarMgr::extractCmdResponse(uint16_t *cmd, uint32_t* data)
{
    if(cmd_recv[0]!=0xFD || cmd_recv[1]!= 0xFC || cmd_recv[2]!=0xFB || cmd_recv[3]!=0xFA) {
        //ESP_LOGE(TAG, "Error recv data format (cmd = %u).", cmd);
        for(int i =0 ; i< 7; i++) printf("%x ", cmd_recv[i]);
        return -1;
    }
    *cmd = cmd_recv[6];
    switch(cmd_recv[6]) {
        case RD03_CMD_OPEN:
            if(cmd_recv[8]==0x0 && cmd_recv[9]==0x0) return 0;
            break;
        case RD03_CMD_CLOSE:
        case 0x07:
        case 0x12:
            if(cmd_recv[8]==0x0 && cmd_recv[9]==0x0) return 0;
            break;
        case 0x08:
            if(cmd_recv[8]==0x0 && cmd_recv[9]==0x0) {
                *data = (cmd_recv[13] << 24) | (cmd_recv[12] << 16) | (cmd_recv[11] << 8) | cmd_recv[10];
                return 0;
            }
    }
    return -1;
}

int RadarMgr::sendCmdPacket(uint16_t len)
{
    int rc = 0;
    acquireUARTDevice();

    for(int i =0 ; i< len; i++) printf("%x ", cmd_tpl[i]);
    printf("\n");
    
    rc = uart_write_bytes(RADARMGR_UART_PORT_NUM, cmd_tpl, len);
    if(rc < len) {
        ESP_LOGE(TAG, "Failed to send data to device (cmd = 0x%x, rc = %d).", RADAR_GET_CMD(), rc);
        return ESP_FAIL;
    }
    if(RADAR_GET_CMD()==RD03_CMD_OPEN) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        uart_flush(RADARMGR_UART_PORT_NUM);
        rc = uart_write_bytes(RADARMGR_UART_PORT_NUM, cmd_tpl, len);
        if(rc < len) {
            ESP_LOGE(TAG, "Failed to send data to device (cmd = 0x%x, rc = %d).", RADAR_GET_CMD(), rc);
            return ESP_FAIL;
        }
    }
    memset(cmd_recv, 0, UART_PACKET_LEN);
    int n = 0;
    rc = uart_read_bytes(RADARMGR_UART_PORT_NUM, cmd_recv, 6, 200);
    if(rc < 6) {
        ESP_LOGE(TAG, "Failed to receive data header length from device (cmd = 0x%x, rc = %d).", RADAR_GET_CMD(), rc);
        return ESP_FAIL;
    }
    n = RADAR_GET_LEN(cmd_recv)+4;
    ESP_LOGI(TAG, "n = %d.", n);
    rc = uart_read_bytes(RADARMGR_UART_PORT_NUM, cmd_recv+6, n, 200);
    if(rc < n) {
        ESP_LOGE(TAG, "Failed to receive data from device (cmd = 0x%x, rc = %d).", RADAR_GET_CMD(), rc);
        return ESP_FAIL;
    }
    for(int i =0 ; i< n+6; i++) printf("%x ", cmd_recv[i]);
    printf("\n");

    uint16_t cmd = 0;
    uint32_t data = 0;
    rc = extractCmdResponse(&cmd, &data);
    if(rc<0) return ESP_FAIL;
    ESP_LOGW(TAG, "cmd = 0x%x, data = %lu", cmd, data);
    return data;
}

/*
 @nTimes: the times to try to detect movement
 @distance: the distance of the moving object in the last detection
 @return: how many times that an movement was detected
*/
int RadarMgr::scan(int nTimes, int *distance)
{
    int rc = 0;
    int n = 0;
    uint8_t *p = cmd_recv;
    uart_flush(RADARMGR_UART_PORT_NUM);
    while(1) {
        rc = uart_read_bytes(RADARMGR_UART_PORT_NUM, p, 1, 200);
        if(rc < 1){
            if(nTimes--==0) break;      //not wait more than nTimes*200ms
            continue;
        } 
        if(*p==0x0A){
            if(p > cmd_recv) *(p-1) = '\0';  //set 0xD to 0x00
            if(memcmp(cmd_recv, "ON", strlen("ON"))==0) {
                ESP_LOGW(TAG, "Movement detected.");
                n++;
            }else if(memcmp(cmd_recv, "Range", strlen("Range"))==0){
                p = cmd_recv + strlen("Range") + 1;
                *distance = atoi((char*)p);
                ESP_LOGW(TAG, "Movement distance is %dcm.", *distance);
            }else if(memcmp(cmd_recv, "OFF", strlen("OFF"))==0){

            }
            if(nTimes--==0) break;
            p = cmd_recv;
            continue;
        }
        p++;
        if(p - cmd_recv > UART_PACKET_LEN) {
            p = cmd_recv;
        }
    }
    return n;
}

int RadarMgr::getDelay()
{
    sendCmdPacket(RADAR_CMD_OPEN());
    sendCmdPacket(RADAR_CMD_GET_TGT_DELAY());
    return sendCmdPacket(RADAR_CMD_CLOSE());
}

int RadarMgr::setDelay(uint16_t delay)
{
    sendCmdPacket(RADAR_CMD_OPEN());
    sendCmdPacket(RADAR_CMD_SET_TGT_DELAY(delay));
    return sendCmdPacket(RADAR_CMD_CLOSE());
}

int RadarMgr::enableDetect()
{
    return sendCmdPacket(RADAR_CMD_CLOSE());
}

int RadarMgr::disableDetect()
{
    return sendCmdPacket(RADAR_CMD_OPEN());
}

