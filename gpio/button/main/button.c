#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gpio.h"

void app_main(void)
{
    config_gpio();

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
