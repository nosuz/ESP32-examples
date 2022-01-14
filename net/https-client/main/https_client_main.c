/* This code is from ESP-IDF example.

   This code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "gpio.h"
#include "nvs.h"
#include "wifi.h"
#include "https_client.h"

static const char *TAG = "wifi_main";

void app_main(void)
{
    nvs_init();
    config_gpio();

    if (wifi_init())
    {
        ESP_LOGW(TAG, "NVS error or cleared and start WPS.");
        wifi_wps_start();
    }
    else
    {
        ESP_LOGI(TAG, "Try to connect");
        wifi_connect();
    }

    while (1)
    {
        ESP_LOGI(TAG, "Wait connection");
        if (wifi_wait_connection())
        {
            https_with_url("https://www.yahoo.co.jp");
            vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
            https_with_url("https://www.google.com/");
        }
        else
        {
            ESP_LOGW(TAG, "Failed to connect");
        }

        wifi_disconnect();

#ifdef CONFIG_LOOP_ACCESS
        ESP_LOGI(TAG, "Total Heap Size: %d", xPortGetFreeHeapSize()); // heap_caps_get_free_size(MALLOC_CAP_8BIT)
        ESP_LOGI(TAG, "Largest Heap Size: %d KiB", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);

        vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);

        wifi_connect();
#else
        break;
#endif
    }

    while (1)
    {
        vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
    }
}
