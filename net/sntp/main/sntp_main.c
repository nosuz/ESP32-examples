// #include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"

#include "gpio.h"
#include "nvs.h"
#include "wifi.h"

#include "ns_sntp.h"

static const char *TAG = "sntp_main";

// Keep value while deep sleeping.
RTC_DATA_ATTR static int boot_count = 0;
RTC_DATA_ATTR static time_t last_synced = 0;

void app_main(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

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
        if (((now - last_synced) > 59) || (timeinfo.tm_year < (2016 - 1900)))
        {
            ESP_LOGW(TAG, "Update systemtime.");
            start_sntp();
            time(&now);
            last_synced = now;
        }
        else
        {
            ESP_LOGI(TAG, "Skip updating systemtime.");
        }

        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Local date/time: %s", strftime_buf);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to connect");
    }

    wifi_disconnect();

    const int deep_sleep_sec = 40;
    ESP_LOGI(TAG, "Entering deep sleep for %d sec.", deep_sleep_sec);
    esp_deep_sleep(1000000LL * deep_sleep_sec);
}
