#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "blink";

#define GPIO_LED CONFIG_BLINK_GPIO
#define PERIOD CONFIG_BLINK_PERIOD

static void config_gpio(void)
{
    ESP_LOGI(TAG, "Setup LED pin.");
    gpio_reset_pin(GPIO_LED);
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
}

static void blink_led(void)
{
    static uint8_t led_state = 0;

    ESP_LOGI(TAG, "Blink LED.");
    gpio_set_level(GPIO_LED, led_state);
    led_state ^= 0x01;
    // led_state = !led_state;
}

void app_main(void)
{
    config_gpio();

    while (1)
    {
        blink_led();
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}
