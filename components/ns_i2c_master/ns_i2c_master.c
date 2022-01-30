#include <stdio.h>
#include "esp_log.h"

#include "ns_i2c_master.h"

#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

static const char *TAG = "i2c_master";

esp_err_t init_i2c_master(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_I2C_MASTER_SDA,
        .scl_io_num = CONFIG_I2C_MASTER_SCL,
#ifdef CONFIG_I2C_PULLUP
        .sda_pullup_en = true,
        .scl_pullup_en = true,
#else
        .sda_pullup_en = false,
        .scl_pullup_en = false,
#endif
        .master.clk_speed = CONFIG_I2C_FREQ_HZ * 1000,
    };

    i2c_param_config(CONFIG_I2C_PORT, &conf);
    // set Max. I2C clock stretch time.
    i2c_set_timeout(CONFIG_I2C_PORT, 0xFFFFF);

    return i2c_driver_install(CONFIG_I2C_PORT, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

esp_err_t delete_i2c_master(void)
{
    return i2c_driver_delete(CONFIG_I2C_PORT);
}

esp_err_t write_i2c_master(uint8_t address, uint8_t data[], size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // Start condition
    i2c_master_start(cmd);
    // Address + Write bit
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, I2C_ACK_EN);
    i2c_master_write(cmd, data, size, I2C_ACK_EN);
    // Stop condition
    i2c_master_stop(cmd);
    // Execute and return status, should return 0
    esp_err_t ret = i2c_master_cmd_begin(CONFIG_I2C_PORT, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t read_i2c_master(uint8_t address, uint8_t data[], size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // Start condition
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, I2C_ACK_EN);
    i2c_master_read(cmd, data, size, I2C_MASTER_ACK);
    // Stop condition
    i2c_master_stop(cmd);
    // Execute and return status, should return 0
    esp_err_t ret = i2c_master_cmd_begin(CONFIG_I2C_PORT, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t reset_i2c_devices_by_general_call(void)
{
    uint8_t data[2];
    data[0] = 0x00;
    data[1] = 0x06;

    esp_err_t ret = write_i2c_master(0x00, data, 2);
    ESP_LOGD(TAG, "Rreset by general call result: %04X", ret);
    return ret;
}
