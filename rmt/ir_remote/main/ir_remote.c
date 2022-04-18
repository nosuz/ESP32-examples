#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_timer.h"
#include "driver/rmt.h"
#include "freertos/semphr.h"

#include "ir_encoder.h"
#include "ir_decoder.h"

#define BUTTON_PIN 5

static const char *TAG = "main";

static rmt_channel_t rx_channel = 0;
static rmt_channel_t tx_channel = 1;

static TimerHandle_t press_timer;

char *json = NULL;
SemaphoreHandle_t send_semaphore = NULL;

void button_press_handler(void *arg)
{
    // expired timer
    if (!gpio_get_level(BUTTON_PIN))
    {
        ESP_LOGI(TAG, "Button pressed");
        xSemaphoreGive(send_semaphore);
    }
    else
    {
        ESP_LOGI(TAG, "Button released");
    }
}

IRAM_ATTR static void pin_intr_handler(void *arg)
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
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);

    press_timer = xTimerCreate("PRESS_TIMER", 5, pdFALSE, NULL, button_press_handler);

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(BUTTON_PIN, pin_intr_handler, NULL); // don't need gpio_intr_enable()
}

void send_ir_data(void *arg)
{
    rmt_item32_t *items = NULL;
    size_t length = 0;

    send_semaphore = xSemaphoreCreateBinary();

    // setup triger button
    config_gpio();
    // setup IR send port
    rmt_config_t rmt_tx_config = RMT_DEFAULT_CONFIG_TX(2, tx_channel);
    rmt_tx_config.tx_config.carrier_en = true;
    rmt_config(&rmt_tx_config);
    rmt_driver_install(tx_channel, 0, 0);

    while (1)
    {
        // wait triger
        xSemaphoreTake(send_semaphore, portMAX_DELAY);
        ESP_LOGI(TAG, "Trigered sending IR data");

        // parse JSON
        if (json != NULL)
            ESP_LOGI(TAG, "Send: %s", json);
        decode_ir_pulses(json, &items, &length);
        ESP_LOGI(TAG, "Items: %d (%p)", length, items);
        if (items == NULL)
            continue;

        // confirm sending pulses
        ESP_LOGI(TAG, "Send IR data");
        for (int i = 0; i < length; i++)
        {
            ESP_LOGI(TAG, "ON: %d, OFF: %d",
                     items[i].duration0,
                     items[i].duration1);
        }
        rmt_write_items(tx_channel, items, length, true); // true: block untile completed transfer.
        free(items);
        items = NULL;
    }
}

void app_main(void)
{
    xTaskCreate(send_ir_data, "send_ir_data", 2048, NULL, 10, NULL);

    RingbufHandle_t rb = NULL;
    rmt_item32_t *items = NULL;
    size_t length = 0;

    rmt_config_t rmt_rx_config = RMT_DEFAULT_CONFIG_RX(19, rx_channel);
    rmt_config(&rmt_rx_config);
    rmt_driver_install(rx_channel, 1000, 0);

    rmt_get_ringbuf_handle(rx_channel, &rb);

    rmt_rx_start(rx_channel, true);
    while (1)
    {
        items = (rmt_item32_t *)xRingbufferReceive(rb, &length, portMAX_DELAY);
        if (items)
        {
            char *encoded = encode_ir_pulses(items, length);
            if (encoded != NULL)
            {
                ESP_LOGI(TAG, "%s", encoded);
                if (json != NULL)
                    free(json);
                json = encoded;
            }
            vRingbufferReturnItem(rb, (void *)items);
        }
    }
}
