#include <stdio.h>
#include <unistd.h>

// // include before freertos/task.h
// #include <freertos/FreeRTOS.h>
// #include "freertos/task.h"
#include "esp_log.h"

#include "ns_crc8.h"
#include "ns_sht4x.h"

#define SHT4X_ADDR 0x44

#if defined(CONFIG_SHT4X_HIGH_REPEATABILITY)
#define MEASURE_COMMAND 0xFD
#define MEASURE_DURATION 9
#elif defined(CONFIG_SHT4X_MEDIUM_REPEATABILITY)
#define MEASURE_COMMAND 0xF6
#define MEASURE_DURATION 5
#elif defined(CONFIG_SHT4X_LOE_REPEATABILITY)
#define MEASURE_COMMAND 0xE0
#define MEASURE_DURATION 2
#endif

#define SERIAL_NUMBER_COMMAND 0x89
#define RESET_COMMAND 0x94

static const char *TAG = "sht40";

esp_err_t sht4x_reset(void)
{
    uint8_t data[0];
    data[0] = RESET_COMMAND;

    esp_err_t ret = write_i2c_master(SHT4X_ADDR, data, 1);
    ESP_LOGD(TAG, "Reset result: %04X", ret);
    usleep(1000); // wait 1ms.

    return ret;
}

esp_err_t sht4x_read_id(uint32_t *id)
{
    uint8_t data[6];
    data[0] = SERIAL_NUMBER_COMMAND;

    I2C_DATA i2c_data;
    esp_err_t ret;

    ret = write_i2c_master(SHT4X_ADDR, data, 1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Read id command error: %04X", ret);
        return ret;
    }

    // Waiting Time: Min. 1ms.
    usleep(1000);

    ret = read_i2c_master_ending_nack(SHT4X_ADDR, data, 6);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Read id value error: %04X", ret);
        return ret;
    }

    i2c_data.data.high = data[0];
    i2c_data.data.low = data[1];
    if (i2c_crc8_check(i2c_data.value, data[2]))
    {
        ESP_LOGE(TAG, "CRC error at read id_H: %02X", data[2]);
        ret = ESP_ERR_INVALID_CRC;
    }
    else
    {
        *id = (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16;
    }

    i2c_data.data.high = data[3];
    i2c_data.data.low = data[4];
    if (i2c_crc8_check(i2c_data.value, data[5]))
    {
        ESP_LOGE(TAG, "CRC error at read id_L: %02X", data[5]);
        ret = ESP_ERR_INVALID_CRC;
    }
    else
    {
        *id |= (uint32_t)data[3] << 8 | (uint32_t)data[4];
    }

    return ret;
}

esp_err_t sht4x_read_sensor(float *temp, float *humid)
{
    uint8_t data[6];
    data[0] = MEASURE_COMMAND;

    I2C_DATA i2c_data;
    esp_err_t ret;

    ret = write_i2c_master(SHT4X_ADDR, data, 1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Measure command error: %04X", ret);
        return ret;
    }

    usleep(MEASURE_DURATION * 1000);

    ret = read_i2c_master_ending_nack(SHT4X_ADDR, data, 6);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Measure value error: %04X", ret);
        return ret;
    }

    i2c_data.data.high = data[0];
    i2c_data.data.low = data[1];
    if (i2c_crc8_check(i2c_data.value, data[2]))
    {
        ESP_LOGE(TAG, "CRC error at read id_H: %02X", data[2]);
        ret = ESP_ERR_INVALID_CRC;
    }
    else
    {
        if (temp != NULL)
        {
            *temp = -45 + 175 * ((float)data[0] * 256 + data[1]) / 65535;
        }
    }

    i2c_data.data.high = data[3];
    i2c_data.data.low = data[4];
    if (i2c_crc8_check(i2c_data.value, data[5]))
    {
        ESP_LOGE(TAG, "CRC error at read id_L: %02X", data[5]);
        ret = ESP_ERR_INVALID_CRC;
    }
    else
    {
        if (humid != NULL)
        {
            *humid = -6 + 125 * ((float)data[3] * 256 + data[4]) / 65535;
            if (*humid > 100)
            {
                *humid = 100;
            }
            else if (*humid < 0)
            {
                *humid = 0;
            }
        }
    }

    return ret;
}
