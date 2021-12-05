#include "esp_err.h"
#include "driver/i2c.h"

#define AQM0802A_ADDR 0x3E
#define DISP_X_SIZE 8
#define DISP_Y_SIZE 2
#define DISP_DDRAM_OFFSET 0x40

#define CHECK_ACK 1

#define CONTROL_BYTE 0x00
#define DATA_BYTE 0x40

esp_err_t aqm0802a_send_command(i2c_port_t port, uint8_t command)
{
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Start condition
    i2c_master_start(cmd);
    // Address + Write bit
    i2c_master_write_byte(cmd, (AQM0802A_ADDR << 1) | I2C_MASTER_WRITE, CHECK_ACK);
    i2c_master_write_byte(cmd, CONTROL_BYTE, CHECK_ACK);
    i2c_master_write_byte(cmd, command, CHECK_ACK);
    i2c_master_stop(cmd);
    // Execute and return status, should return 0
    ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t aqm0802a_write_data(i2c_port_t port, uint8_t data)
{
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Start condition
    i2c_master_start(cmd);
    // Address + Write bit
    i2c_master_write_byte(cmd, (AQM0802A_ADDR << 1) | I2C_MASTER_WRITE, CHECK_ACK);
    i2c_master_write_byte(cmd, DATA_BYTE, CHECK_ACK);
    i2c_master_write_byte(cmd, data, CHECK_ACK);
    i2c_master_stop(cmd);
    // Execute and return status, should return 0
    ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

void aqm0802a_clear_display(i2c_port_t port)
{
    aqm0802a_send_command(port, 0x01);
    ets_delay_us(2 * 1000);
}

void aqm0802a_return_home(i2c_port_t port)
{
    aqm0802a_send_command(port, 0x02);
    ets_delay_us(2 * 1000);
}

void aqm0802a_display_on(i2c_port_t port)
{
    aqm0802a_send_command(port, 0b00001100); // ON with no cursor, no blink
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

void aqm0802a_display_off(i2c_port_t port)
{
    aqm0802a_send_command(port, 0b00001000); // ON with cursor, no blink
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

void aqm0802a_set_contrast(i2c_port_t port, uint8_t contrast)
{
    aqm0802a_send_command(port, 0x39); /* extension mode */
    ets_delay_us(50);

    aqm0802a_send_command(port, 0x70 | (contrast & 0x0F));
    ets_delay_us(50);
    aqm0802a_send_command(port, 0x54 | (contrast >> 4 & 0x03));
    ets_delay_us(50);

    aqm0802a_send_command(port, 0x38); /* normal mode */
    ets_delay_us(50);
}

void aqm0802a_move_cursor(i2c_port_t port, uint8_t x, uint8_t y)
{
    uint8_t addr;

    x %= DISP_X_SIZE;
    y %= DISP_Y_SIZE;
    addr = x + DISP_DDRAM_OFFSET * y;

    aqm0802a_send_command(port, 0x80 | addr);
    ets_delay_us(50);
}

void aqm0802a_print(i2c_port_t port, char *string)
{
    for (uint8_t i = 0; i < DISP_X_SIZE; i++)
    {
        if (string[i] == 0x00)
        {
            break;
        }
        else if (string[i] == 0x0A)
        {
            /* next line */
        }
        else
        {
            aqm0802a_write_data(port, string[i]);
            ets_delay_us(20);
        }
    }
}

void aqm0802a_init(i2c_port_t port)
{
    vTaskDelay(40 / portTICK_PERIOD_MS);

    //aqm0802a_send_command(port, 0x38); /* normal mode */
    //ets_delay_us(27);

    aqm0802a_send_command(port, 0x39); /* extension mode */
    ets_delay_us(27);
    aqm0802a_send_command(port, 0x14);
    ets_delay_us(27);
    aqm0802a_send_command(port, 0x70); /* contrast set c3-c0 */
    ets_delay_us(27);
    aqm0802a_send_command(port, 0x56); /* booster on and contrast set c5, c4 */
    ets_delay_us(27);
    aqm0802a_send_command(port, 0x6C);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    aqm0802a_send_command(port, 0x38); /* normal mode */
    ets_delay_us(27);
    aqm0802a_send_command(port, 0x0C); /* display on and cursor off */
    ets_delay_us(27);

    aqm0802a_clear_display(port);
}
