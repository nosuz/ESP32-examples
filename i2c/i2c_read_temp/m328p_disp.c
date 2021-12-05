#include "i2c_disp.h"

#ifdef SOFT_I2C
#include "soft_i2c.h"
#else
#include "i2c.h"
#endif

#include <util/delay.h>

void disp_send_command(uint8_t data) {
  if (i2c_start_write(DISP_I2C_ADDR) == 0) {
    i2c_put(0x00);
    i2c_put(data);
  }
  i2c_stop();
}

void disp_send_data(uint8_t data) {
  if (i2c_start_write(DISP_I2C_ADDR) == 0) {
    i2c_put(0x40);
    i2c_put(data);
  }
  i2c_stop();
}

void disp_send_string(char *string, uint8_t length) {
  uint8_t x;

  if (length == 0) {
    length = DISP_X_SIZE;
  }
  if (i2c_start_write(DISP_I2C_ADDR) == 0) {
    i2c_put(0x40);
    for(x = 0; x < length; x++) {
      if (string[x] == 0x00) {
	break;
      } else {
	i2c_put(string[x]);
	_delay_us(50);
      }
    }
  }
  i2c_stop();
}

void disp_init() {
  i2c_init();

  _delay_ms(1000);

#if defined(AQM0802A)
  /*
    AQM0802A require initalize Power and Foller control
  */
  disp_send_command(0x39); // Function set N=1(2-line mode), DH=0, RE=0, IS=1
  _delay_us(30);
  //disp_send_command(0x14);
  //_delay_us(30);
  disp_send_command(0x6C); // Follower control
  _delay_ms(200);
  disp_send_command(0x38); // Function set N=1(2-line mode), DH=0, RE=0, IS=0
  _delay_us(30);

  disp_set_contrast(0x20); // 0x00 to 0x3F, default 0x20
#elif defined(SO1602A)
  disp_set_contrast(0x7F); // 0x00 to 0xFF, default 0x7F
#endif

  disp_clear_display();
  disp_return_home();
  disp_display_on();
  disp_clear_display();
}

void disp_clear_display() {
  disp_send_command(0x01);
  _delay_ms(2);
}

void disp_return_home() {
  disp_send_command(0x02);
  _delay_ms(2);
}

void disp_display_on() {
  disp_send_command(0b00001100); // ON with no cursor, no blink
  _delay_us(50);
}

void disp_display_off() {
  disp_send_command(0b00001000); // ON with cursor, no blink
  _delay_us(50);
}

void disp_set_contrast(uint8_t contrast) {
#if defined(AQM0802A)
  disp_send_command(0x39); // Function set N=1(2-line mode), DH=0, RE=0, IS=1
  _delay_us(50);
  disp_send_command(0x70|(contrast & 0x0F)); // Contrast set C3:0
  _delay_us(50);
  disp_send_command(0x54|(contrast >> 4 & 0x03)); // Contrast set C5:4
  _delay_us(50);
  disp_send_command(0x38); // Function set N=1(2-line mode), DH=0, RE=0, IS=0
  _delay_us(50);
#elif defined(SO1602A)
  disp_send_command(0x3A); // Function set N=1(2-line mode), DH=0, RE=1, IS=0
  _delay_us(50);
  disp_send_command(0x79); // OLED Char SD=1
  _delay_us(50);
  disp_send_command(0x81); // Contrast set C3:0
  _delay_us(50);
  disp_send_command(contrast); // Contrast set C5:4
  _delay_us(50);
  disp_send_command(0x78); // OLED Char SD=0
  _delay_us(50);
  disp_send_command(0x38); // Function set N=1(2-line mode), DH=0, RE=0, IS=0
  _delay_us(50);
#endif
}

void disp_move_cursor(uint8_t x, uint8_t y) {
  uint8_t addr;

  x %= DISP_X_SIZE;
  y %= DISP_Y_SIZE;
  addr = x + DISP_DDRAM_OFFSET * y;

  disp_send_command(0x80|addr);
  _delay_us(50);
}

void disp_write(uint8_t data) {
  disp_send_data(data);
  _delay_us(50);
}

void disp_print(char *string, uint8_t y) {
  disp_move_cursor(0, y);
  disp_send_string(string, 0);
}

void disp_update(char *buffer) {
  uint8_t y;

  for(y = 0; y < DISP_Y_SIZE; y++) {
    disp_move_cursor(0, y);
    disp_send_string(buffer + y * DISP_X_SIZE, DISP_X_SIZE);
  }
}
