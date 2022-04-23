#include "ns_i2c_master.h"

esp_err_t adt7410_set_configure(uint8_t config);
esp_err_t adt7410_set_high_resoluton(void);
esp_err_t adt7410_set_low_resoluton(void);
esp_err_t adt7410_read_temp(float *temp);
