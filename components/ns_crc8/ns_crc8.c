#include <stdio.h>

#include "ns_crc8.h"

uint8_t crc8_i2c(uint8_t *buf, size_t size)
{
    uint8_t crc = 0xff; // init
    uint8_t i, j;
    for (i = 0; i < size; i++)
    {
        crc ^= buf[i];
        for (j = 0; j < 8; j++)
        {
            if ((crc & 0x80) != 0)
                crc = (uint8_t)((crc << 1) ^ 0x31); // X8 + X5 + X4 + 1
            else
                crc <<= 1;
        }
    }
    return crc;
}

esp_err_t i2c_crc8_check(uint16_t value, uint8_t crc)
{
    // return T if CRC error
    uint8_t buf[2];
    buf[0] = (value >> 8);
    buf[1] = (value & 0xff);

    return (crc8_i2c(buf, 2) == crc) ? ESP_OK : ESP_ERR_INVALID_CRC;
}