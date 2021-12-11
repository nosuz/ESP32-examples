#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"

#include "esp_sleep.h"
#include "freertos/task.h"

#if IDF_TARGET == esp32c3
#include "driver/temp_sensor.h"
#endif

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

    union
    {
        uint16_t temp;
        struct temp_struct
        {
            // Little Endian
            uint8_t high;
            uint8_t low;
        } data;

    } i2c_data;

    float temp;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Start condition
    i2c_master_start(cmd);
    // Address + Write bit
    i2c_master_write_byte(cmd, (ADT7410_ADDR << 1) | I2C_MASTER_WRITE, ACK_EN);
    i2c_master_write_byte(cmd, ADT7410_TEMP_MSB, ACK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADT7410_ADDR << 1) | I2C_MASTER_READ, ACK_EN);
    i2c_master_read_byte(cmd, &i2c_data.data.low, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &i2c_data.data.high, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    // Execute and return status, should return 0
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    printf("I2C result: %02X", ret);
    i2c_cmd_link_delete(cmd);

    printf("%02X", i2c_data.data.low);
    printf(" %02X >> ", i2c_data.data.high);
    printf("%04X\n", i2c_data.temp);

    temp = 0.0625 * (i2c_data.temp >> 3); // 13-bits

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

#if IDF_TARGET == esp32c3
    temp_sensor_config_t internal_temp_sensor = {
        .dac_offset = TSENS_DAC_L2,
        .clk_div = 6,
    };
    temp_sensor_set_config(internal_temp_sensor);
#endif

    while (1)
    {
        ESP_LOGI(TAG, "Read sendor");
        cur_tmp = read_adt7410();
        /*
        esp_sleep_enable_timer_wakeup(2 * 1000 * 1000);
        esp_light_sleep_start();
        */
        aqm0802a_clear_display(I2C_MASTER_NUM);
        sprintf(temp_str, "Te=%.1fC", cur_tmp);
        aqm0802a_print(I2C_MASTER_NUM, temp_str);

#if IDF_TARGET == esp32c3
        float tsens_out;
        temp_sensor_read_celsius(&tsens_out);
        ESP_LOGI(TAG, "Inner Temp.: %.1f", tsens_out);

        aqm0802a_move_cursor(I2C_MASTER_NUM, 0, 1);
        sprintf(temp_str, "Ti=%.1fC", tsens_out);
        aqm0802a_print(I2C_MASTER_NUM, temp_str);
#endif
        vTaskDelay(2 * 1000 / portTICK_PERIOD_MS);

        aqm0802a_clear_display(I2C_MASTER_NUM);
        vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
    }

    ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));
    ESP_LOGI(TAG, "I2C unitialized successfully");
}
