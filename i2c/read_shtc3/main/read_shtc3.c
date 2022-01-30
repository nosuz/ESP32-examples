#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ns_shtc3.h"
#include "ns_adt7410.h"

static const char *TAG = "main";

void app_main(void)
{
    uint16_t sensor_id = 0;

    init_i2c_master();

    shtc3_reset();

    if (shtc3_read_id(&sensor_id))
    {
        ESP_LOGE(TAG, "CRC error on read sensor ID.");
    }
    else
    {
        ESP_LOGI(TAG, "ID: %04X", sensor_id);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    float temperature = 0;
    float humidity = 0;
    while (1)
    {
        shtc3_read_sensor(&temperature, &humidity);
        printf("Temp.: %.01fC, Humi: %.01f%%\n", temperature, humidity);

        printf("temp %0.1f\n", adt7410_read_temp());

        vTaskDelay(pdMS_TO_TICKS(5 * 1000));
    }

    delete_i2c_master();
}
