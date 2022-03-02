#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"

#include "driver/gpio.h"
#include "driver/rtc_io.h"

#include "driver/touch_sensor.h"

#define GPIO_LED CONFIG_LED_PIN

#define TOUCH_PAD CONFIG_TOUCH_PAD
#define TOUCH_THRESHOLD (80)
#define FILTER_PERIOD (10)

#define ON true
#define OFF false

const char *TAG = "Dsleep";

void config_gpio(void)
{
    ESP_LOGI(TAG, "Setup LED pin.");
    gpio_reset_pin(GPIO_LED);
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
}

void set_led(bool status)
{
    gpio_set_level(GPIO_LED, status ? 1 : 0);
}

#ifdef CONFIG_WAKEUP_TOUCH
volatile bool touched = false;

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
    uint16_t touch_thresh_value = touch_value * TOUCH_THRESHOLD / 100;
    ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_PAD, touch_thresh_value));
}

void touch_intr_handler(void *arg)
{
    touch_pad_clear_status();
    touched = true;
    touch_pad_intr_disable();
}
#endif

void app_main(void)
{
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_EXT0:
        ESP_LOGI(TAG, "Wakeup by EXT0");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        ESP_LOGI(TAG, "Wakeup by EXT1");
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(TAG, "Wakeup by Timer");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        ESP_LOGI(TAG, "Wakeup by Touchpad");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        ESP_LOGI(TAG, "Wakeup by ULP");
        break;
    default:
        ESP_LOGI(TAG, "Wakeup by unkown reason");
        break;
    }

    config_gpio();

#ifdef CONFIG_WAKEUP_TOUCH
    init_touch_pad();
    touch_pad_isr_register(touch_intr_handler, NULL);
    touch_pad_intr_enable();
#endif

    ESP_LOGI(TAG, "LED ON");
    set_led(ON);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));
#ifdef CONFIG_WAKEUP_TOUCH
    if (touched)
        ESP_LOGI(TAG, "Touched");
#endif
    ESP_LOGI(TAG, "LED OFF");
    set_led(OFF);

    // #include "driver/rtc_io.h"
    rtc_gpio_isolate(GPIO_LED);

#ifdef CONFIG_WAKEUP_GPIO
    rtc_gpio_pullup_en(CONFIG_TRIGER_PIN);
    rtc_gpio_pulldown_dis(CONFIG_TRIGER_PIN);
    esp_err_t err = esp_sleep_enable_ext0_wakeup(CONFIG_TRIGER_PIN, 0);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "GPIO%d is not available for Wakeup triger", CONFIG_TRIGER_PIN);
#endif

#ifdef CONFIG_WAKEUP_TOUCH
    esp_sleep_enable_touchpad_wakeup();
#endif

#ifdef CONFIG_WAKEUP_GPIO
    ESP_LOGI(TAG, "Deep sleep untile triger by GPIO");
#endif
#ifdef CONFIG_WAKEUP_TOUCH
    ESP_LOGI(TAG, "Deep sleep untile triger by Touchpad");
#endif
#ifdef CONFIG_WAKEUP_TIMER
    ESP_LOGI(TAG, "Or wake up after %d sec.", CONFIG_SLEEP_DURATION);
    esp_sleep_enable_timer_wakeup(1000000LL * CONFIG_SLEEP_DURATION);
#endif
    esp_deep_sleep_start();
}
