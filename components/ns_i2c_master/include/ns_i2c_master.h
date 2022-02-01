#include "driver/i2c.h"

#ifndef NS_I2C_MASTER_H
#define NS_I2C_MASTER_H

#define I2C_ACK_EN 1

typedef union
{
    uint16_t value;
    struct data_struct
    {
        // ESP32 is Little Endian
        uint8_t low;
        uint8_t high;
    } data;
} I2C_DATA;

esp_err_t init_i2c_master(void);
esp_err_t delete_i2c_master(void);
esp_err_t write_i2c_master(uint8_t address, uint8_t data[], size_t size);
esp_err_t read_i2c_master(uint8_t address, uint8_t data[], size_t size);
esp_err_t read_i2c_master_ending_nack(uint8_t address, uint8_t data[], size_t size);
esp_err_t reset_i2c_devices_by_general_call(void);

#endif
