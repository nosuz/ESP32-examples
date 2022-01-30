// https://sensirion.com/media/documents/643F9C8E/6164081E/Sensirion_Humidity_Sensors_SHTC3_Datasheet.pdf

#include <stdio.h>
#include <unistd.h>
#include "esp_log.h"

#include "ns_crc8.h"
#include "ns_shtc3.h"

#define SHTC3_ADDR 0x70

#define COMMAND_RESET 0x805D
#define COMMAND_SLEEP 0xB098
#define COMMAND_WAKEUP 0x3517

#define COMMAND_READ_ID 0xEFC8

#ifdef CONFIG_SHTC3_USE_STRETCH
// Use clock stretch
#ifdef CONFIG_SHTC3_LOW_POWER_MODE
#define COMMAND_READ_SENSOR 0x6458 // Read temperature first, Low power, clock stretch
#else
#define COMMAND_READ_SENSOR 0x7CA2 // Read temperature first, clock stretch
#endif
#else
// Wait specified time
#ifdef CONFIG_SHTC3_LOW_POWER_MODE
#define COMMAND_READ_SENSOR 0x609C // No clock stretch, Read temperature first, Low power.
#else
#define COMMAND_READ_SENSOR 0x7866 // No clock stretch, Read temperature first.
#endif
#endif

static const char *TAG = "shtc3";

esp_err_t shtc3_reset(void)
{
    uint8_t data[2];
    data[0] = (COMMAND_RESET >> 8);
    data[1] = (COMMAND_RESET & 0xFF);

    esp_err_t ret = write_i2c_master(SHTC3_ADDR, data, 2);
    ESP_LOGD(TAG, "Reset result: %04X", ret);
    usleep(240); // wait 240us.

    return ret;
}

esp_err_t shtc3_sleep(void)
{
    uint8_t data[2];
    data[0] = (COMMAND_SLEEP >> 8);
    data[1] = (COMMAND_SLEEP & 0xFF);

    esp_err_t ret = write_i2c_master(SHTC3_ADDR, data, 2);
    ESP_LOGD(TAG, "Sleep result: %04X", ret);

    return ret;
}

esp_err_t shtc3_wakeup(void)
{

    uint8_t data[2];
    data[0] = (COMMAND_WAKEUP >> 8);
    data[1] = (COMMAND_WAKEUP & 0xFF);

    esp_err_t ret = write_i2c_master(SHTC3_ADDR, data, 2);
    ESP_LOGD(TAG, "Wakeup result: %04X", ret);
    usleep(240); // wait 240us.

    return ret;
}

esp_err_t shtc3_read_id(uint16_t *id)
{
    I2C_DATA i2c_data;
    esp_err_t ret;

    uint8_t data[3];
    data[0] = (COMMAND_READ_ID >> 8);
    data[1] = (COMMAND_READ_ID & 0xFF);

    ret = write_i2c_master(SHTC3_ADDR, data, 2);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Read id command error: %04X", ret);
        return ret;
    }

    // vTaskDelay(1);

    ret = read_i2c_master(SHTC3_ADDR, data, 3);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Read id value error: %04X", ret);
        return ret;
    }

    i2c_data.data.high = data[0];
    i2c_data.data.low = data[1];
    if (i2c_crc8_check(i2c_data.value, data[2]))
    {
        ESP_LOGE(TAG, "CRC error at read id: %02X", data[2]);
        ret = ESP_ERR_INVALID_CRC;
    }
    else
    {
        *id = i2c_data.value;
    }

    return ret;
}

esp_err_t shtc3_read_sensor(float *temperature, float *humidity)
{

    I2C_DATA i2c_temp_data;
    I2C_DATA i2c_humi_data;
    esp_err_t ret;

    uint8_t data[6];
    data[0] = (COMMAND_READ_SENSOR >> 8);
    data[1] = (COMMAND_READ_SENSOR & 0xFF);

    ret = write_i2c_master(SHTC3_ADDR, data, 2);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Read sensor command error: %04X", ret);
        return ret;
    }

#ifndef CONFIG_SHTC3_USE_STRETCH
    usleep(11 * 1000); // wait 11ms.
#endif

    ret = read_i2c_master(SHTC3_ADDR, data, 6);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Read sendor value result: %04X", ret);
        return ret;
    }

    i2c_temp_data.data.high = data[0];
    i2c_temp_data.data.low = data[1];
    if (i2c_crc8_check(i2c_temp_data.value, data[2]))
    {
        ESP_LOGE(TAG, "CRC error at read temperature: %02X", data[2]);
        ret = ESP_ERR_INVALID_CRC;
    }
    else
    {
        if (temperature != NULL)
            *temperature = -45.0 + 175.0 * i2c_temp_data.value / 65536;
    }

    i2c_humi_data.data.high = data[3];
    i2c_humi_data.data.low = data[4];
    if (i2c_crc8_check(i2c_humi_data.value, data[5]))
    {
        ESP_LOGE(TAG, "CRC error at read humidity: %02X", data[5]);
        ret = ESP_ERR_INVALID_CRC;
    }
    else
    {
        if (humidity != NULL)
        {
            *humidity = 100.0 * i2c_humi_data.value / 65536;
            // Relative humidity must be between 0 and 100
            if (*humidity > 100)
            {
                *humidity = 100;
            }
            else if (*humidity < 0)
            {
                *humidity = 0;
            }
        }
    }

    return ret;
}
