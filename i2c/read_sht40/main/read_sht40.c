#include <stdio.h>
// include before freertos/task.h
#include <freertos/FreeRTOS.h>
#include "freertos/task.h"

#include "esp_log.h"

#include "ns_sht4x.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(init_i2c_master());
    ESP_LOGI(TAG, "I2C initialized successfully");

    ESP_ERROR_CHECK(sht4x_reset());

    uint32_t serial = 0;
    ESP_ERROR_CHECK(sht4x_read_id(&serial));
    printf("Serial: %08X\n", serial);

    while (1)
    {
        float temp, humid;

        ESP_ERROR_CHECK(sht4x_read_sensor(&temp, &humid));
        printf("Tm=%.1fC\n", temp);
        printf("Hm=%.1f%%\n", humid);
        vTaskDelay(pdMS_TO_TICKS(5 * 1000));
    }
}
