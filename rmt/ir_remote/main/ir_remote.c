#include <stdio.h>

#include "esp_log.h"
#include "driver/rmt.h"

#include "ir_encoder.h"

static const char *TAG = "main";

static rmt_channel_t rx_channel = 0;

void app_main(void)
{
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
            char *json = encode_ir_pulses(items, length);
            if (json != NULL)
            {
                ESP_LOGI(TAG, "%s", json);
                free(json);
            }
            vRingbufferReturnItem(rb, (void *)items);
        }
    }
}
