#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "spi.h"
#include "ADT7310.h"

static const char *TAG = "main";

void app_main(void)
{
    init_spi();
    attach_adt7310();
    // wait filling current temp into registor.
    vTaskDelay(pdMS_TO_TICKS(1000)); // wait 1 sec.

    while (1)
    {
        ESP_LOGI(TAG, "device config: %02x", adt7310_read_config());
        uint8_t id = adt7310_read_id();
        ESP_LOGI(TAG, "device id: %02X", id);
        printf("Temp: %.1fC\n", adt7310_read_temp());

        vTaskDelay(pdMS_TO_TICKS(2 * 1000)); // wait 2 sec.
    }
}
