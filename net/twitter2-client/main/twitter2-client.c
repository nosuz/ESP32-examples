#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_sleep.h"

#include "ns_wifi.h"
#include "ns_sntp.h"
#include "ns_twitter2.h"

static const char *TAG = "twitter-client";

void app_main(void)
{
    ESP_LOGI(TAG, "Init Wifi");
    wifi_init();
    ESP_LOGI(TAG, "Try to connect");
    wifi_connect();

    ESP_LOGI(TAG, "Wait connection");
    if (wifi_wait_connection())
    {
        if (twitter2_init() == ESP_OK)
        {
            twitter2_api_init();
            twitter2_api_param("text", "本日は晴天なり。");
            twitter2_update_status();
        }
        else
        {
            ESP_LOGE(TAG, "Failed to init Twitter");
        }
    }
    wifi_disconnect();

    esp_deep_sleep_start();
}
