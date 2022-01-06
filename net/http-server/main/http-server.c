#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "gpio.h"
#include "nvs.h"
#include "wifi.h"
#include "http_serve.h"
#include "m_dns.h"
#include "spiffs.h"

static const char *TAG = "server_main";

void app_main(void)
{
    httpd_handle_t server;

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

    ESP_ERROR_CHECK(init_spiffs());

    do
    {
        ESP_LOGI(TAG, "Wait connection");
        if (wifi_wait_connection())
        {
            ESP_LOGI(TAG, "Start mDNS service");
            start_mdns_service();

            ESP_LOGI(TAG, "Start HTTP server");
            server = start_webserver();

            // wait until restart
            wifi_wait_restart();

            if (server)
            {
                ESP_LOGI(TAG, "Stop HTTP server");
                stop_webserver(server);
            }
            ESP_LOGI(TAG, "Stop mDNS service");
            stop_mdns_service();
        }
        else
        {
            ESP_LOGE(TAG, "Failed to make connection");
            vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
        }

    } while (1);
}
