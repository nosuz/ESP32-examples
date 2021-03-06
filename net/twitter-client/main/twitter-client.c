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

    ESP_LOGI(TAG, "Init Wifi");
    wifi_init();
    ESP_LOGI(TAG, "Try to connect");
    wifi_connect();

    ESP_LOGI(TAG, "Wait connection");
    if (wifi_wait_connection())
    {
        // Get systemtime into `now` and set local time in `timeinfo`
        ESP_LOGW(TAG, "Update systemtime.");
        init_sntp(3600);
        sync_sntp();
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Local date/time: %s", strftime_buf);

        strftime(strftime_buf, sizeof(strftime_buf), "Hello Ladies + Gentlemen, it's %c", &timeinfo);
        twitter_init_api_params();
        twitter_set_api_param("status", strftime_buf);
        twitter_set_api_param("include_entities", "true");
        twitter_update_status();

        vTaskDelay(pdMS_TO_TICKS(2 * 1000));
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "It's %c", &timeinfo);

        twitter_init_api_params();
        twitter_set_api_param("status", strftime_buf);
        twitter_update_status();
    }
    else
    {
        ESP_LOGW(TAG, "Failed to connect");
    }

    wifi_disconnect();
}
