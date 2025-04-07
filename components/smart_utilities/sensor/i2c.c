#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_event.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "i2c.h"

static const char *TAG = "I2C_C";

void i2c_init(uint8_t i2c_master_port, uint8_t sda_io_num, uint8_t scl_io_num)
{
	esp_err_t res;

	i2c_config_t conf = {
	    .mode = I2C_MODE_MASTER,
	    .sda_io_num = sda_io_num,         		// select GPIO specific to your project
	    .sda_pullup_en = GPIO_PULLUP_ENABLE,
	    .scl_io_num = scl_io_num,         		// select GPIO specific to your project
	    .scl_pullup_en = GPIO_PULLUP_ENABLE,
	    .master.clk_speed = I2C_MASTER_FREQ_HZ,  // select frequency specific to your project
	};

	res = i2c_param_config(i2c_master_port, &conf);
	if (res != ESP_OK) ESP_LOGW(TAG, "Error in i2c_param_config()\r");

	res = i2c_driver_install(i2c_master_port, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
	if (res != ESP_OK) ESP_LOGW(TAG, "Error in i2c_driver_install()");
}

esp_err_t i2c_write_byte(uint8_t i2c_master_port, uint8_t address, uint8_t command, uint8_t data)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, command, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(i2c_master_port, cmd, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	if (ret == ESP_OK) {
		//printf("i2c_write_byte successful\r\n");
	} else {
		ESP_LOGW(TAG, "i2c_write_byte failed");
	}

	return(ret);
}

esp_err_t i2c_write_short(uint8_t i2c_master_port, uint8_t address, uint8_t command, uint16_t data)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, command, ACK_CHECK_EN);

	i2c_master_write_byte(cmd, (data & 0xFF00) >> 8, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, data & 0xFF, ACK_CHECK_EN);
	i2c_master_stop(cmd);

	esp_err_t ret = i2c_master_cmd_begin(i2c_master_port, cmd, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	if (ret == ESP_OK) {
		//printf("i2c_write successful\r\n");
	} else {
		ESP_LOGW(TAG, "i2c_write_short failed (0x%x)", address);
	}

	return(ret);
}

esp_err_t i2c_write_buf(uint8_t i2c_master_port, uint8_t address, uint8_t command, uint8_t *data, uint8_t len)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, command, ACK_CHECK_EN);
	if (len) {
		for (int i = 0; i < len; i++) {
			i2c_master_write_byte(cmd, data[i], ACK_CHECK_EN);
		}
	}
	i2c_master_stop(cmd);

	esp_err_t ret = i2c_master_cmd_begin(i2c_master_port, cmd, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	if (ret == ESP_OK) {
		//printf("i2c_write successful\r\n");
	} else {
		ESP_LOGW(TAG, "i2c_write_buf failed (0x%x)", address);
	}

	return(ret);
}

uint8_t i2c_read_byte(uint8_t i2c_master_port, uint8_t address, uint8_t command)
{
	i2c_write_buf(i2c_master_port, address, command, NULL, 0);

	uint8_t data;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
	i2c_master_read_byte(cmd, &data, NACK_VAL);
	i2c_master_stop(cmd);

	esp_err_t ret = i2c_master_cmd_begin(i2c_master_port, cmd, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	if (ret == ESP_OK) {
		//printf("i2c_read_byte successful\r\n");
	} else if (ret == ESP_ERR_TIMEOUT) {
		ESP_LOGW(TAG, "i2c_read_byte timeout");
	} else {
		ESP_LOGW(TAG, "i2c_read_byte failed");
	}
	return(data);
}

uint16_t i2c_read_short(uint8_t i2c_master_port, uint8_t address, uint8_t command)
{
	i2c_write_buf(i2c_master_port, address, command, NULL, 0);

	uint16_t data;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
	i2c_master_read(cmd, (uint8_t *)&data, 2, ACK_VAL);
	i2c_master_stop(cmd);

	esp_err_t ret = i2c_master_cmd_begin(i2c_master_port, cmd, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);


	if (ret == ESP_OK) {

	} else if (ret == ESP_ERR_TIMEOUT) {
		ESP_LOGW(TAG, "Bus is busy (0x%x)", address);
		return 0;
	} else {
		ESP_LOGW(TAG, "Read failed (0x%x)", address);
		return 0;
	}
	return(__bswap16(data));
}

esp_err_t i2c_read_buf(uint8_t i2c_master_port, uint8_t address, uint8_t command, uint8_t *buffer, uint8_t len)
{
	i2c_write_buf(i2c_master_port, address, command, NULL, 0);

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
	i2c_master_read(cmd, buffer, len, ACK_VAL);
	i2c_master_stop(cmd);

	esp_err_t ret = i2c_master_cmd_begin(i2c_master_port, cmd, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	if (ret == ESP_OK) {
		for (int i = 0; i < len; i++) {
			//printf("0x%02x ", data[i]);
		}
	} else if (ret == ESP_ERR_TIMEOUT) {
		ESP_LOGW(TAG, "Bus is busy (0x%x)", address);
		return 0;
	} else {
		ESP_LOGW(TAG, "Read failed (0x%x)", address);
		return 0;
	}
	return(ret);
}
