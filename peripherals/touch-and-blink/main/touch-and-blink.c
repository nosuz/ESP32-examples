#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"

#include "driver/touch_sensor.h"
#include "driver/gpio.h"

#define TOUCH_PAD CONFIG_TOUCH_PAD
#define GPIO_LED CONFIG_GPIO_LED

#define TOUCH_THRESHOLD (80)
#define FILTER_PERIOD (10)

static const char *TAG = "touch_blink";

volatile static int touched_count = 0;
uint16_t touch_thresh_value;
TimerHandle_t timer;

void config_gpio(void)
{
    ESP_LOGI(TAG, "Setup LED pin.");
    gpio_reset_pin(GPIO_LED);
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
}

void toggle_led(void)
{
    ESP_LOGI(TAG, "ToggleLED.");
    gpio_set_level(GPIO_LED, touched_count % 2);
}

void init_touch_pad(void)
{
    uint16_t touch_value;

    ESP_LOGI(TAG, "Init touch pad");
    ESP_ERROR_CHECK(touch_pad_init());
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);

    touch_pad_config(TOUCH_PAD, 0);
    touch_pad_filter_start(FILTER_PERIOD);

    touch_pad_read_filtered(TOUCH_PAD, &touch_value);
    ESP_LOGI(TAG, "init touch value: %d", touch_value);
    touch_thresh_value = touch_value * TOUCH_THRESHOLD / 100;
    ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_PAD, touch_thresh_value));
}

void touch_timer_expired(void)
{
    ESP_LOGI(TAG, "touch released");
    toggle_led();
}

void touch_intr_handler(void *arg)
{
    touch_pad_clear_status();
    // touch_pad_intr_disable();

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xTimerIsTimerActive(timer) == pdFALSE)
    {
        touched_count++;
        xTimerStartFromISR(timer, &xHigherPriorityTaskWoken);
    }
    else
    {
        xTimerResetFromISR(timer, &xHigherPriorityTaskWoken);
    }
}

void app_main(void)
{
    init_touch_pad();
    config_gpio();

    touch_pad_isr_register(touch_intr_handler, NULL);

    uint16_t touch_value;
    timer = xTimerCreate("TOUCH_TIMER", pdMS_TO_TICKS(50), pdFALSE, NULL, touch_timer_expired);
    touch_pad_intr_enable();

    while (1)
    {
        printf("Touched: %d\n", touched_count);
        vTaskDelay(pdMS_TO_TICKS(1000));
        // vTaskDelay(1000/ portTICK_PERIOD_MS);
        touch_pad_read_filtered(TOUCH_PAD, &touch_value);
        ESP_LOGI(TAG, "touch value: %d", touch_value);
    }
}
