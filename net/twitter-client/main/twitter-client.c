#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

#include "ns_wifi.h"
#include "ns_sntp.h"
#include "ns_twitter.h"

static const char *TAG = "twitter-client";

void app_main(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    nvs_init();
    config_gpio();

    init_sntp();

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
        // Get systemtime into `now` and set local time in `timeinfo`
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year < (2016 - 1900))
        {
            ESP_LOGW(TAG, "Update systemtime.");
            start_sntp();
            time(&now);
        }
        else
        {
            ESP_LOGI(TAG, "Skip updating systemtime.");
        }

        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Local date/time: %s", strftime_buf);

        twitter_init_api_params();
        twitter_set_api_param("status", "Hello Ladies + Gentlemen, a signed OAuth request!");
        twitter_set_api_param("include_entities", "true");

        twitter_update_status();
    }
    else
    {
        ESP_LOGW(TAG, "Failed to connect");
    }

    wifi_disconnect();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5 * 1000));
    }
}
