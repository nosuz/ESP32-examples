#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "wifi.h"

#define BUTTON_PIN CONFIG_BUTTON_PIN
// #define GPIO_INPUT_PIN_SEL (1ULL << BUTTON_PIN)
#define LONG_PRESS (CONFIG_LONG_DURATION * 1000 * 1000)

#define GPIO_LED CONFIG_LED_PIN
#define BLINK_TIMER (500 / 2)

static char *TAG = "gpio";

TimerHandle_t press_timer;
TimerHandle_t blink_timer;

void start_blink(void)
{
    xTimerStart(blink_timer, 0);
}

void stop_blink(void)
{
    gpio_set_level(GPIO_LED, 0);
    xTimerStop(blink_timer, 0);
}

void blink_led(void)
{
    static uint8_t led_state = 0;
    gpio_set_level(GPIO_LED, led_state);
    led_state ^= 0x01;

    xTimerReset(blink_timer, 0);
}

void button_press_handler(void *arg)
{
    static int prev_level = 1;
    static int64_t pressed_at = 0;

    int64_t pressed_time;
    bool repeat_timer = true;

    int cur_level = gpio_get_level(BUTTON_PIN);
    if (cur_level == 0)
    {
        // pressed
        if (cur_level != prev_level)
        {
            pressed_at = esp_timer_get_time();
        }
        else
        {
            pressed_time = esp_timer_get_time() - pressed_at;
            if (pressed_time > LONG_PRESS)
            {
                ESP_LOGI(TAG, "Long Pressed");
                repeat_timer = false;
                wifi_wps_start();
            }
        }
    }
    else
    {
        // released
        repeat_timer = false;
        if (cur_level != prev_level)
        {
            pressed_time = esp_timer_get_time() - pressed_at;
            printf("Pushed button: %lld ms\n", pressed_time / 1000);
        }
    }
    prev_level = cur_level;

    if (repeat_timer)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTimerReset(press_timer, &xHigherPriorityTaskWoken);
    }
}

// IRAM_ATTR
static void
pin_intr_handler(void *arg)
{
    ets_printf("button INT\n");

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTimerResetFromISR(press_timer, &xHigherPriorityTaskWoken);
}

void config_gpio(void)
{
    ESP_LOGI(TAG, "Setup button pin (%d)", BUTTON_PIN);
    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    // gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_ANYEDGE); // triger if pressed
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);
    // gpio_config_t in_conf = {
    //     //disable interrupt
    //     .intr_type = GPIO_INTR_NEGEDGE,
    //     //set as input mode
    //     .mode = GPIO_MODE_INPUT,
    //     //bit mask of the pins that you want to set
    //     .pin_bit_mask = GPIO_INPUT_PIN_SEL,
    //     //disable pull-down mode
    //     .pull_down_en = 0,
    //     //enable pull-up mode
    //     .pull_up_en = 1};
    // //configure GPIO with the given settings
    // gpio_config(&in_conf);

    ESP_LOGI(TAG, "Setup LED pin.");
    gpio_reset_pin(GPIO_LED);
    gpio_set_pull_mode(GPIO_LED, GPIO_FLOATING);
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);

    press_timer = xTimerCreate("PRESS_TIMER", 5, pdFALSE, NULL, button_press_handler);
    // blink_timer expires every 500ms.
    blink_timer = xTimerCreate("LED_TIMER", BLINK_TIMER / portTICK_PERIOD_MS, pdFALSE, NULL, blink_led);

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(BUTTON_PIN, pin_intr_handler, (void *)BUTTON_PIN); // don't need gpio_intr_enable()
}

int read_gpio(void)
{
    return gpio_get_level(BUTTON_PIN);
}
