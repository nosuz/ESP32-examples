#include "esp_types.h"

uint8_t crc8_i2c(uint8_t *buf, uint8_t size)
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
