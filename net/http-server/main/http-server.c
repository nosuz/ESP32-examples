#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "gpio.h"
#include "nvs.h"
#include "wifi.h"
#include "http_serve.h"

static const char *TAG = "server_main";

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

    ESP_LOGI(TAG, "Wait connection");
    if (wifi_wait_connection())
    {
        ESP_LOGI(TAG, "HTTP server start");
        start_webserver();
    }

    while (1)
    {
        vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
    }
}
