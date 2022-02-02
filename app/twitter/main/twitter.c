#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_sleep.h"

#include "ns_adc.h"
#include "ns_adt7410.h"
#include "ns_shtc3.h"

#include "ns_wifi.h"
#include "ns_sntp.h"
#include "ns_twitter.h"

#define TWEET_BUF_SIZE 200
#define STRFTIME_SIZE 64

static const char *TAG = "main";

RTC_DATA_ATTR static int boot_count = 0;

char tweet[TWEET_BUF_SIZE];

void app_main(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[STRFTIME_SIZE];

    uint32_t voltage = 0;
    float battery;
    float adt7410_temp = 0;
    float shtc3_temp = 0;
    float shtc3_humi = 0;

    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    // setenv("TZ", "JST-9", 1);
    tzset();

    init_adc();
    init_i2c_master();

    time(&now);
    localtime_r(&now, &timeinfo);

    read_adc_voltage(&voltage);
    battery = (float)voltage * 3 / 1000; // (100k + 200k) / 100k = 3
    printf("Battery: %0.2f V\n", battery);

    adt7410_read_temp(&adt7410_temp);
    shtc3_read_sensor(&shtc3_temp, &shtc3_humi);

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

        strftime(strftime_buf, sizeof(strftime_buf), "%H時%m分", &timeinfo);

        snprintf(tweet, TWEET_BUF_SIZE,
                 "自動更新(%d分毎): %s現在\n"
                 "室温1: %.01f℃, 室温2: %.01f℃, 湿度: %.0f%%\n"
                 "電圧: %.02fV\n"
                 "起動%d回目\n"
                 "Tweet from #ESP32",
                 CONFIG_SLEEP_LENGTH,
                 strftime_buf,
                 adt7410_temp,
                 shtc3_temp, shtc3_humi,
                 battery,
                 boot_count);
        printf("%s\n", tweet);

        twitter_set_api_param("status", tweet);
        // twitter_set_api_param("include_entities", "true");

        twitter_update_status();
    }
    else
    {
        ESP_LOGW(TAG, "Failed to connect");
    }

    wifi_disconnect();

    // Termination voltaege for Ni-MH is 1.0V and 4.0v for 4 batteries.
    if (battery <= 4.0)
    {
        // never wakeup.
        ESP_LOGW(TAG, "Entering deep sleep. Never wakeup because low battery: %.01f", battery);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    }
    else
    {
        ESP_LOGI(TAG, "Entering deep sleep for %d min.", CONFIG_SLEEP_LENGTH);
        esp_sleep_enable_timer_wakeup(1000000LL * CONFIG_SLEEP_LENGTH * 60);
        // esp_deep_sleep(1000000LL * SLEEP_MIN);
    }
    esp_deep_sleep_start();
}
