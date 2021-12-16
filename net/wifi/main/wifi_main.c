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

#include "nvs.h"
#include "wifi.h"
#include "http_get.h"

static const char *TAG = "wifi_main";

void app_main(void)
{
    nvs_init();

    wifi_init();

    while (1)
    {
        ESP_LOGI(TAG, "Try to connect");
        wifi_connect();
        if (!wifi_wait_connection())
        {
            break;
        }
        run_http_get();
        vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
        wifi_disconnect();
        ESP_LOGI(TAG, "Disconnected");
        vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
    }
}
