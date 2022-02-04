void init_adc(void);
uint32_t read_adc_raw(void);
esp_err_t read_adc_voltage(uint32_t *voltage);
uint32_t calc_adc_voltage(uint32_t raw);
bool calc_adc_voltage_available(void);
