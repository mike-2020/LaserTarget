#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2s_types.h"
#include <string.h>
#include "board.h"
#include "audio_error.h"
#include "audio_mem.h"

static const char *TAG = "MY_BOARD_V1_0";

esp_err_t get_i2c_pins(i2c_port_t port, i2c_config_t *i2c_config)
{
    AUDIO_NULL_CHECK(TAG, i2c_config, return ESP_FAIL);
    if (port == I2C_NUM_0 || port == I2C_NUM_1) {
        i2c_config->sda_io_num = -1;
        i2c_config->scl_io_num = -1;
    } else {
        i2c_config->sda_io_num = -1;
        i2c_config->scl_io_num = -1;
        ESP_LOGE(TAG, "i2c port %d is not supported", port);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_i2s_pins(int port, board_i2s_pin_t *i2s_config)
{
    AUDIO_NULL_CHECK(TAG, i2s_config, return ESP_FAIL);
    if (port == 0) {
        i2s_config->mck_io_num = -1;
        i2s_config->bck_io_num = GPIO_NUM_35;
        i2s_config->ws_io_num = GPIO_NUM_36;
        i2s_config->data_out_num = GPIO_NUM_2;
        i2s_config->data_in_num = -1;
    } else if (port == 1) {
        i2s_config->bck_io_num = -1;
        i2s_config->ws_io_num = -1;
        i2s_config->data_out_num = -1;
        i2s_config->data_in_num = -1;
    } else {
        memset(i2s_config, -1, sizeof(board_i2s_pin_t));
        ESP_LOGE(TAG, "i2s port %d is not supported", port);
        return ESP_FAIL;
    }

    return ESP_OK;
}

int8_t get_pa_enable_gpio(void)
{
    return PA_ENABLE_GPIO; 
}


esp_err_t get_spi_pins(spi_bus_config_t *spi_config, spi_device_interface_config_t *spi_device_interface_config)
{
    AUDIO_NULL_CHECK(TAG, spi_config, return ESP_FAIL);
    AUDIO_NULL_CHECK(TAG, spi_device_interface_config, return ESP_FAIL);

    spi_config->mosi_io_num = -1;
    spi_config->miso_io_num = -1;
    spi_config->sclk_io_num = -1;
    spi_config->quadwp_io_num = -1;
    spi_config->quadhd_io_num = -1;

    spi_device_interface_config->spics_io_num = -1;

    ESP_LOGW(TAG, "SPI interface is not supported");
    return ESP_OK;
}

/*
esp_err_t i2s_mclk_gpio_select(i2s_port_t i2s_num, gpio_num_t gpio_num)
{
    if (i2s_num >= I2S_NUM_MAX) {
        ESP_LOGE(TAG, "Does not support i2s number(%d)", i2s_num);
        return ESP_ERR_INVALID_ARG;
    }
    if (gpio_num != GPIO_NUM_0 && gpio_num != GPIO_NUM_1 && gpio_num != GPIO_NUM_3) {
        ESP_LOGE(TAG, "Only support GPIO0/GPIO1/GPIO3, gpio_num:%d", gpio_num);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "I2S%d, MCLK output by GPIO%d", i2s_num, gpio_num);
    if (i2s_num == I2S_NUM_0) {
        if (gpio_num == GPIO_NUM_0) {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
            WRITE_PERI_REG(PIN_CTRL, 0xFFF0);
        } else if (gpio_num == GPIO_NUM_1) {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
            WRITE_PERI_REG(PIN_CTRL, 0xF0F0);
        } else {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
            WRITE_PERI_REG(PIN_CTRL, 0xFF00);
        }
    } else if (i2s_num == I2S_NUM_1) {
        if (gpio_num == GPIO_NUM_0) {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
            WRITE_PERI_REG(PIN_CTRL, 0xFFFF);
        } else if (gpio_num == GPIO_NUM_1) {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
            WRITE_PERI_REG(PIN_CTRL, 0xF0FF);
        } else {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
            WRITE_PERI_REG(PIN_CTRL, 0xFF0F);
        }
    }
    return ESP_OK;
}
*/