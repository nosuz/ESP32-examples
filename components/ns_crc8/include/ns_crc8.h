#include "esp_err.h"

uint8_t crc8_i2c(uint8_t *buf, size_t size);
esp_err_t i2c_crc8_check(uint16_t value, uint8_t crc);
