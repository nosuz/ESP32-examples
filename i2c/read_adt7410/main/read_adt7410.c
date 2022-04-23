#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ns_adt7410.h"

static const char *TAG = "main";

void app_main(void)
{
    uint16_t sensor_id = 0;

    init_i2c_master();

    float temp = 0;
    while (1)
    {
        adt7410_set_high_resoluton();
        adt7410_read_temp(&temp);
        printf("ADT7410(HiRes) %fC\n", temp);

        vTaskDelay(pdMS_TO_TICKS(5 * 1000));

        adt7410_set_low_resoluton();
        adt7410_read_temp(&temp);
        printf("ADT7410(LowRes) %fC\n", temp);

        vTaskDelay(pdMS_TO_TICKS(5 * 1000));
    }

    delete_i2c_master();
}
