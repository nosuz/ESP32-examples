
#include "esp_err.h"
#include "driver/i2c.h"

esp_err_t aqm0802a_send_command(i2c_port_t port, uint8_t command);
esp_err_t aqm0802a_write_data(i2c_port_t port, uint8_t data);
void aqm0802a_init(i2c_port_t port);
void aqm0802a_clear_display(i2c_port_t port);
void aqm0802a_return_home(i2c_port_t port);
void aqm0802a_display_on(i2c_port_t port);
void aqm0802a_display_off(i2c_port_t port);
void aqm0802a_set_contrast(i2c_port_t port, uint8_t contrast);
void aqm0802a_move_cursor(i2c_port_t port, uint8_t x, uint8_t y);
void aqm0802a_print(i2c_port_t port, char *string);
