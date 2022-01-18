void attach_adt7310(void);
void adt7310_reset(void);
uint8_t adt7310_read_id(void);
uint8_t adt7310_read_config(void);
void adt7310_write_config(uint8_t command);
float adt7310_read_temp(void);
