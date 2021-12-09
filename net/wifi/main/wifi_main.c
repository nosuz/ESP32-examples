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
#include "nvs_flash.h"

#include "wifi.h"

static const char *TAG = "wifi_main";

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();

    while (1)
    {
        ESP_LOGI(TAG, "Try to connect");
        wifi_connect();
        wifi_wait_connection();
        vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
        wifi_disconnect();
        ESP_LOGI(TAG, "Disconnected");
        vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
    }
}
