#include "i2c_master.h"

void aqm0802a_init();
void aqm0802a_clear_display();
void aqm0802a_return_home();
void aqm0802a_display_on();
void aqm0802a_display_off();
void aqm0802a_set_contrast(uint8_t contrast);
void aqm0802a_move_cursor(uint8_t x, uint8_t y);
void aqm0802a_print(char *string);
