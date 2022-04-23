#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ns_adt7410.h"

#define ACK_EN 1

#if defined(CONFIG_ADT7410_AT_0x48)
#define ADT7410_ADDR 0x48
#elif defined(CONFIG_ADT7410_AT_0x49)
#define ADT7410_ADDR 0x49
#elif defined(CONFIG_ADT7410_AT_0x4A)
#define ADT7410_ADDR 0x4A
#elif defined(CONFIG_ADT7410_AT_0x4B)
#define ADT7410_ADDR 0x4B
#endif

#define ADT7410_TEMP_MSB 0x00

static const char *TAG = "adt7410";

esp_err_t adt7410_read_temp(float *temp)
{
    esp_err_t ret;
    I2C_DATA i2c_data;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Start condition
    i2c_master_start(cmd);
    // Address + Write bit
    i2c_master_write_byte(cmd, (ADT7410_ADDR << 1) | I2C_MASTER_WRITE, ACK_EN);
    i2c_master_write_byte(cmd, ADT7410_TEMP_MSB, ACK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADT7410_ADDR << 1) | I2C_MASTER_READ, ACK_EN);
    i2c_master_read_byte(cmd, &i2c_data.data.high, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &i2c_data.data.low, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    // Execute and return status, should return 0
    ret = i2c_master_cmd_begin(CONFIG_I2C_PORT, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Read id command error: %04X", ret);
        return ret;
    }

    int16_t value = ((int16_t)i2c_data.value) >> 3;
    *temp = (float)value * 0.0625;

    return ret;
}
