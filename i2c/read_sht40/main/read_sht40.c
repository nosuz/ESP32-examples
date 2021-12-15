#include <stdio.h>
// include before freertos/task.h
#include <freertos/FreeRTOS.h>
#include "freertos/task.h"

#include "esp_log.h"

#include "i2c.h"
#include "SHT40.h"

static const char *TAG = "read_sht40";

void app_main(void)
{
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");
    // SHT40_soft_reset();

    uint32_t serial = SHT_serail_number();
    printf("Serial: %08X\n", serial);

    while (1)
    {
        float temp, humid;

        SHT40_read_sensor(&temp, &humid);
        printf("Tm=%.1fC\n", temp);
        printf("Hm=%.1f%%\n", humid);
        vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
    }
}
