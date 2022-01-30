#include "ns_i2c_master.h"

esp_err_t shtc3_reset(void);
esp_err_t shtc3_read_id(uint16_t *id);
esp_err_t shtc3_read_sensor(float *humidity, float *temperature);
esp_err_t shtc3_sleep(void);
esp_err_t shtc3_wakeup(void);
