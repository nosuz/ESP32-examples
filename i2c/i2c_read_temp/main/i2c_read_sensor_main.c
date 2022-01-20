#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"

#include "esp_sleep.h"
#include "freertos/task.h"

#if CONFIG_IDF_TARGET_ESP32C3
#include "driver/temp_sensor.h"
#endif

#include "adt7410.h"
#include "aqm0802a.h"

static const char *TAG = "i2c-read-sensor";

void app_main(void)
{
    float cur_tmp = 0;
    char temp_str[8];

    ESP_ERROR_CHECK(init_i2c_master());
    ESP_LOGI(TAG, "I2C initialized successfully");

    ESP_LOGI(TAG, "I2C initialized successfully");
    aqm0802a_init();

#if CONFIG_IDF_TARGET_ESP32C3
    temp_sensor_config_t internal_temp_sensor = {
        .dac_offset = TSENS_DAC_L2,
        .clk_div = 6,
    };
    temp_sensor_set_config(internal_temp_sensor);
#endif

    while (1)
    {
        ESP_LOGI(TAG, "Read sendor");
        cur_tmp = adt7410_read_temp();
        /*
        esp_sleep_enable_timer_wakeup(2 * 1000 * 1000);
        esp_light_sleep_start();
        */
        aqm0802a_clear_display();
        sprintf(temp_str, "Te=%.1fC", cur_tmp);
        aqm0802a_print(temp_str);

#if CONFIG_IDF_TARGET_ESP32C3
        float tsens_out;
        temp_sensor_read_celsius(&tsens_out);
        ESP_LOGI(TAG, "Inner Temp.: %.1f", tsens_out);

        aqm0802a_move_cursor(0, 1);
        sprintf(temp_str, "Ti=%.1fC", tsens_out);
        aqm0802a_print(temp_str);
#endif
        vTaskDelay(pdMS_TO_TICKS(2 * 1000));

        aqm0802a_clear_display();
        vTaskDelay(pdMS_TO_TICKS(2 * 1000));
    }

    ESP_ERROR_CHECK(delete_i2c_master());
    ESP_LOGI(TAG, "I2C unitialized successfully");
}
