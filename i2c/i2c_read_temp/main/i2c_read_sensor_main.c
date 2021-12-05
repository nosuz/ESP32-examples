#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"

#include "esp_sleep.h"
#include "freertos/task.h"

#include "AQM0802A.h"

static const char *TAG = "i2c-read-sensor";

#define I2C_MASTER_SCL_IO CONFIG_I2C_MASTER_SCL /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO CONFIG_I2C_MASTER_SDA /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM 0                        /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ 400000               /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0             /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0             /*!< I2C master doesn't need buffer */
#define ACK_EN 1

#define ADT7410_ADDR 0x48 /*!< Slave address of the ADT7410 sensor */
#define ADT7410_TEMP_MSB 0x00

static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

static float read_adt7410(void)
{
    esp_err_t ret;
    uint8_t data[2];
    uint16_t _temp;
    float temp;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Start condition
    i2c_master_start(cmd);
    // Address + Write bit
    i2c_master_write_byte(cmd, (ADT7410_ADDR << 1) | I2C_MASTER_WRITE, ACK_EN);
    i2c_master_write_byte(cmd, ADT7410_TEMP_MSB, ACK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADT7410_ADDR << 1) | I2C_MASTER_READ, ACK_EN);
    i2c_master_read_byte(cmd, &data[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    // Execute and return status, should return 0
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    printf("I2C result: %02X", ret);
    i2c_cmd_link_delete(cmd);

    printf("%02X", data[0]);
    printf("%02X >> ", data[1]);

    _temp = (data[0] << 8 | data[1]) / 8;
    temp = 0.0625 * _temp;

    printf("%04X\n", _temp);
    printf("%.2f'C\n", 0.0625 * _temp);

    return temp;
}

void app_main(void)
{
    float cur_tmp = 0;
    char temp_str[8];

    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    ESP_LOGI(TAG, "I2C initialized successfully");
    aqm0802a_init(I2C_MASTER_NUM);

    while (1)
    {
        ESP_LOGI(TAG, "Read sendor");
        cur_tmp = read_adt7410();
        /*
        esp_sleep_enable_timer_wakeup(2 * 1000 * 1000);
        esp_light_sleep_start();
        */
        aqm0802a_clear_display(I2C_MASTER_NUM);
        sprintf(temp_str, "Tm=%.1fC", cur_tmp);
        aqm0802a_print(I2C_MASTER_NUM, temp_str);

        vTaskDelay(2 * 1000 / portTICK_PERIOD_MS);
        aqm0802a_clear_display(I2C_MASTER_NUM);
        vTaskDelay(2 * 1000 / portTICK_PERIOD_MS);
    }

    ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));
    ESP_LOGI(TAG, "I2C unitialized successfully");
}
