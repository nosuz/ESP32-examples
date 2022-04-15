#include <stdio.h>
#include <math.h>

#include "esp_log.h"
#include "driver/rmt.h"

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
            int unit = 0;
            int i = 0;
            if (items[0].duration1 != 0)
            {
                for (i = 1; i < length; i++)
                {
                    unit += items[i].duration0;

                    if (items[i].duration1 == 0)
                    {
                        break;
                    }
                }
                unit /= i;
            }
            int pulses = i + 1;

            // if unit is not 0, it must be more than 2 pulses.
            if (unit != 0)
            {
                ESP_LOGI(TAG, "Unit: %d, Pulses: %d", unit, pulses);

                for (int i = 0; i < pulses; i++)
                {
                    ESP_LOGI(TAG, "ON: %d(%d), OFF: %d(%d)",
                             items[i].duration0, (int)round((float)items[i].duration0 / unit),
                             items[i].duration1, (int)round((float)items[i].duration1 / unit));
                }
                ESP_LOGI(TAG, "-----------------------------------");
            }
            vRingbufferReturnItem(rb, (void *)items);
        }
    }
}
