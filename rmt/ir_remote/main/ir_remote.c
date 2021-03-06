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

#define BUTTON_PIN CONFIG_IR_TRIGER_GPIO

static const char *TAG = "main";

static rmt_channel_t tx_channel = CONFIG_IR_TX_CHANNEL;
static rmt_channel_t rx_channel = CONFIG_IR_RX_CHANNEL;

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
    send_semaphore = xSemaphoreCreateBinary();

    // setup triger button
    config_gpio();
    // setup IR send port
    rmt_config_t rmt_tx_config = RMT_DEFAULT_CONFIG_TX(CONFIG_IR_TX_GPIO, tx_channel);
    rmt_tx_config.tx_config.carrier_en = true;
    rmt_config(&rmt_tx_config);
    rmt_driver_install(tx_channel, 0, 0);

    while (1)
    {
        // wait triger
        xSemaphoreTake(send_semaphore, portMAX_DELAY);
        ESP_LOGI(TAG, "Trigered sending IR data");
        rmt_rx_stop(rx_channel);

        // parse JSON
        if (json != NULL)
            ESP_LOGI(TAG, "Send: %s", json);
        RMT_ITEMS *rmt_items = decode_ir_pulses(json);
        ESP_LOGI(TAG, "Items: %d (%p)", rmt_items->length, rmt_items->items);
        if (rmt_items == NULL)
            continue;

        // confirm sending pulses
        ESP_LOGI(TAG, "Send IR data");
        for (int i = 0; i < rmt_items->length; i++)
        {
            ESP_LOGI(TAG, "ON: %d, OFF: %d",
                     rmt_items->items[i].duration0,
                     rmt_items->items[i].duration1);
        }
        rmt_write_items(tx_channel, rmt_items->items, rmt_items->length, true); // true: block untile completed transfer.
        delete_rmt_items(rmt_items);

        vTaskDelay(pdMS_TO_TICKS(100));
        rmt_rx_start(rx_channel, true);
    }
}

void app_main(void)
{
    xTaskCreate(send_ir_data, "send_ir_data", 2048, NULL, 10, NULL);

    RingbufHandle_t rb = NULL;
    rmt_item32_t *items = NULL;
    size_t length = 0;

    rmt_config_t rmt_rx_config = RMT_DEFAULT_CONFIG_RX(CONFIG_IR_RX_GPIO, rx_channel);
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
