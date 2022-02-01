#include "ns_i2c_master.h"

esp_err_t sht4x_reset(void);
esp_err_t sht4x_read_id(uint32_t *id);
esp_err_t sht4x_read_sensor(float *temp, float *humid);
