#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ns_wifi.h"
#include "ns_http.h"

static const char *TAG = "main";

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
            ns_http_get("https://www.yahoo.co.jp");
            vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
            ns_http_get("https://www.google.com/");
        }
        else
        {
            ESP_LOGW(TAG, "Failed to connect");
        }

        wifi_disconnect();
    }

    while (1)
    {
        vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
    }
}
