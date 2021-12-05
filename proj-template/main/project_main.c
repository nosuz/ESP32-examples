#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define GPIO_LED CONFIG_BLINK_GPIO
#define PERIOD CONFIG_BLINK_PERIOD

static const char *TAG = "example";

void app_main(void)
{
}
