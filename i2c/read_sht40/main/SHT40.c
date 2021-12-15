// include before freertos/task.h
#include <freertos/FreeRTOS.h>
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"

#include "SHT40.h"
#include "crc.h"

#define I2C_MASTER_NUM 0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define ACK_EN 1

#define SHT40_ADDR 0x44

#define HIGH_REPEATABILITY_COMMAND 0xFD
#define MEDIUM_REPEATABILITY_COMMAND 0xF6
#define LOW_REPEATABILITY_COMMAND 0xE0
#define SERIAL_NUMBER_COMMAND 0x89
#define RESET_COMMAND 0x94

static const char *TAG = "sht40";

esp_err_t send_command(uint8_t command_code)
{
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    // Address + Write bit
    i2c_master_write_byte(cmd, (SHT40_ADDR << 1) | I2C_MASTER_WRITE, ACK_EN);
    i2c_master_write_byte(cmd, command_code, ACK_EN);
    i2c_master_stop(cmd);
    // Execute and return status, should return 0
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    vTaskDelay(10 / portTICK_PERIOD_MS);

    return ret;
}

esp_err_t read_data(uint8_t *data, uint8_t size)
{
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    // Address + Read bit
    i2c_master_write_byte(cmd, (SHT40_ADDR << 1) | I2C_MASTER_READ, ACK_EN);
    i2c_master_read(cmd, data, size - 1, I2C_MASTER_ACK);
    i2c_master_read(cmd, &data[size - 1], 1, I2C_MASTER_NACK);

    i2c_master_stop(cmd);
    // Execute and return status, should return 0
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

int SHT40_read_sensor(float *temp, float *humid)
{
    uint8_t data[6];
    uint8_t crc;
    int crc_err = 0;

    send_command(HIGH_REPEATABILITY_COMMAND);
    read_data(data, 6);

    crc = crc8_i2c(&data[0], 2);
    // printf("Temp -> DATA: %02X %02X, CRC: %02X ?=? %02X\n", data[0], data[1], data[2], crc);
    if (temp != NULL)
    {
        *temp = -45 + 175 * ((float)data[0] * 256 + data[1]) / 65535;
    }
    if (data[2] != crc)
    {
        ESP_LOGW(TAG, "SHT40 CRC Error, read: %02X, calc: %02X", data[2], crc);
        crc_err = 1;
    }

    crc = crc8_i2c(&data[3], 2);
    // printf("Humid -> DATA: %02X %02X, CRC: %02X ?=? %02X\n", data[3], data[4], data[5], crc);
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
    if (data[5] != crc)
    {
        ESP_LOGW(TAG, "SHT40 CRC Error, read: %02X, calc: %02X", data[5], crc);
        crc_err = 1;
    }

    return crc_err;
}

void SHT40_soft_reset(void)
{
    send_command(RESET_COMMAND);
}

uint32_t SHT_serail_number(void)
{
    uint8_t data[6];
    uint8_t crc;
    bool crc_err = false;

    send_command(SERIAL_NUMBER_COMMAND);
    read_data(data, 6);

    crc = crc8_i2c(&data[0], 2);
    // printf("DATA_H: %02X %02X, CRC: %02X ?=? %02X\n", data[0], data[1], data[2], crc);
    if (data[2] != crc)
    {
        ESP_LOGW(TAG, "SHT40 CRC Error, read: %02X, calc: %02X", data[2], crc);
        crc_err = true;
    }

    crc = crc8_i2c(&data[3], 2);
    // printf("DATA_L: %02X %02X, CRC: %02X ?=? %02X\n", data[3], data[4], data[5], crc);
    if (data[5] != crc)
    {
        ESP_LOGW(TAG, "SHT40 CRC Error, read: %02X, calc: %02X", data[5], crc);
        crc_err = true;
    }

    uint32_t serial_number = (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16 | (uint32_t)data[3] << 8 | (uint32_t)data[4];
    if (crc_err)
    {
        serial_number = 0;
    }

    return serial_number;
}
