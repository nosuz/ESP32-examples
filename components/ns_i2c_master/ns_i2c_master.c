#include <stdio.h>
#include "ns_i2c_master.h"

#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

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

    return i2c_driver_install(CONFIG_I2C_PORT, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

esp_err_t delete_i2c_master(void)
{
    return i2c_driver_delete(CONFIG_I2C_PORT);
}
