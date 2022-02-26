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
#include "ns_twitter2.h"
#include "ns_ambient.h"

#define TWEET_BUF_SIZE 200
#define STRFTIME_SIZE 64

static const char *TAG = "main";

RTC_DATA_ATTR static int boot_count = 0;

RTC_DATA_ATTR static float highest_temp = 0;
RTC_DATA_ATTR static float lowest_temp = 100;
RTC_DATA_ATTR static bool tweeted_lowest_temp = false;
RTC_DATA_ATTR static bool tweeted_highest_temp = false;

char tweet[TWEET_BUF_SIZE];

void app_main(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[STRFTIME_SIZE];

    uint32_t voltage = 0;
    float battery = 0;
    float adt7410_temp = 0;
    float shtc3_temp = 0;
    float shtc3_humi = 0;

    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    set_local_timezone();
    init_adc();
    init_i2c_master();

    read_adc_voltage(&voltage);
    battery = (float)voltage * 3 / 1000; // (100k + 200k) / 100k = 3
    printf("Battery: %0.2f V\n", battery);

    adt7410_read_temp(&adt7410_temp);
    shtc3_read_sensor(&shtc3_temp, &shtc3_humi);

    if (adt7410_temp > highest_temp)
    {
        highest_temp = adt7410_temp;
    }
    if (shtc3_temp > highest_temp)
    {
        highest_temp = shtc3_temp;
    }

    if (adt7410_temp < lowest_temp)
    {
        lowest_temp = adt7410_temp;
    }

    if (shtc3_temp < lowest_temp)
    {
        lowest_temp = shtc3_temp;
    }

    ESP_LOGI(TAG, "Init Wifi");
    wifi_init();
    ESP_LOGI(TAG, "Try to connect");
    wifi_connect();

    ESP_LOGI(TAG, "Wait connection");
    if (wifi_wait_connection())
    {
        // update Ambient
        printf("室温1: %.01f℃, 室温2: %.01f℃, 湿度: %.0f%% "
               "電圧: %.02fV "
               "起動%d回目\n",
               adt7410_temp,
               shtc3_temp, shtc3_humi,
               battery,
               boot_count);

        ambient_set(1, adt7410_temp);
        ambient_set(2, shtc3_temp);
        ambient_set(3, shtc3_humi);
        ambient_set(4, battery);
        ambient_set(5, boot_count);

        ambient_send();

        // update daily lowestlowest temp.
        // highest in April to November.

        if (boot_count == 1)
            twitter2_init();
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Local date/time: %s", strftime_buf);

        if (timeinfo.tm_hour == 0)
        {
            // reset updated tweet
            tweeted_lowest_temp = false;
            tweeted_highest_temp = false;
        }

        tweet[0] = '\0';
        if (!tweeted_lowest_temp && (timeinfo.tm_hour == 7))
        {
            tweeted_lowest_temp = true;

            snprintf(tweet, TWEET_BUF_SIZE,
                     "今日(%d月%d日)の最低気温: %.01f℃\n"
                     "現在の電圧: %.02fV\n"
                     "起動%d回目",
                     timeinfo.tm_mon + 1,
                     timeinfo.tm_mday,
                     lowest_temp,
                     battery,
                     boot_count);
            lowest_temp = 100;
        }
        else if (!tweeted_highest_temp && (timeinfo.tm_hour == 19))
        {
            tweeted_highest_temp = true;
            if ((timeinfo.tm_mon >= 3) && (timeinfo.tm_mon <= 10))
            {
                snprintf(tweet, TWEET_BUF_SIZE,
                         "今日(%d月%d日)の最高気温: %.01f℃\n"
                         "現在の電圧: %.02fV\n"
                         "起動%d回目",
                         timeinfo.tm_mon + 1,
                         timeinfo.tm_mday,
                         highest_temp,
                         battery,
                         boot_count);
            }
            highest_temp = 0;
        }

        if (tweet[0] != '\0')
        {
            printf("Update status: %s\n", tweet);

            if (twitter2_init() == ESP_OK)
            {
                twitter2_api_init();
                twitter2_api_param("text", tweet);
                twitter2_update_status();
            }
            else
            {
                ESP_LOGE(TAG, "Failed to init Twitter");
            }
        }
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
