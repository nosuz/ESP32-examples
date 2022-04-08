#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_sleep.h"
#include "cJSON.h"

#include "ns_adc.h"
#include "ns_adt7410.h"
#include "ns_shtc3.h"

#include "ns_wifi.h"
#include "ns_sntp.h"
#include "ns_twitter2.h"
#include "ns_google_api.h"

#define TWEET_BUF_SIZE 200
#define STRFTIME_SIZE 64
#define VOLTS_HIST 3

#define TWEET_LOWEST_AT 7
#define TWEET_HIGHEST_AT 19

static const char *TAG = "main";

RTC_DATA_ATTR static int boot_count = 0;
RTC_DATA_ATTR static time_t boot_at = 0;

RTC_DATA_ATTR static float highest_temp = 0;
RTC_DATA_ATTR static float lowest_temp = 100;
RTC_DATA_ATTR static bool tweeted_lowest_temp = false;
RTC_DATA_ATTR static bool tweeted_highest_temp = false;

RTC_DATA_ATTR static float volts[VOLTS_HIST] = {0};

static char tweet[TWEET_BUF_SIZE];

void app_main(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[STRFTIME_SIZE];
    // char val_buf[9]; // +100.000_

    uint32_t voltage = 0;
    float battery = 0;
    float mean_battery = 0;
    float adt7410_temp = 0;
    float shtc3_temp = 0;
    float shtc3_humi = 0;
    tweet[0] = '\0';

    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    set_local_timezone();
    time(&now);
    localtime_r(&now, &timeinfo);

    init_adc();
    init_i2c_master();

    read_adc_voltage(&voltage);
    battery = (float)voltage * 3 / 1000; // (100k + 200k) / 100k = 3
    printf("Battery: %0.2f V\n", battery);
    if (boot_count == 1)
    {
        for (int i = 0; i < VOLTS_HIST; i++)
        {
            volts[i] = battery;
        }
    }
    // calc moving average of battery level.
    for (int i = 0; i < VOLTS_HIST; i++)
    {
        mean_battery += volts[i];
        if (i != 0)
            volts[i - 1] = volts[i];
    }
    volts[VOLTS_HIST - 1] = battery;

    mean_battery += battery;
    mean_battery /= (VOLTS_HIST + 1);

    adt7410_read_temp(&adt7410_temp);
    shtc3_read_sensor(&shtc3_temp, &shtc3_humi);

    if (timeinfo.tm_hour < TWEET_LOWEST_AT)
    {
        if (adt7410_temp < lowest_temp)
        {
            lowest_temp = adt7410_temp;
        }

        if (shtc3_temp < lowest_temp)
        {
            lowest_temp = shtc3_temp;
        }
    }

    if (timeinfo.tm_hour < TWEET_HIGHEST_AT)
    {
        if (adt7410_temp > highest_temp)
        {
            highest_temp = adt7410_temp;
        }

        if (shtc3_temp > highest_temp)
        {
            highest_temp = shtc3_temp;
        }
    }

    ESP_LOGI(TAG, "Init Wifi");
    wifi_init();
    ESP_LOGI(TAG, "Try to connect");
    wifi_connect();

    ESP_LOGI(TAG, "Wait connection");
    if (wifi_wait_connection())
    {
        printf("室温1: %.01f℃, 室温2: %.01f℃, 湿度: %.0f%% "
               "電圧: %.02fV "
               "起動%d回目\n",
               adt7410_temp,
               shtc3_temp, shtc3_humi,
               battery,
               boot_count);

        if (boot_count == 1)
            // update clock by SNTP in twitter2_init()
            twitter2_init();
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Local date/time: %s", strftime_buf);
        if (boot_count == 1)
            boot_at = now;

        // update Google Spreadsheet
        ESP_LOGI(TAG, "Append data on Google Spreadsheet");

        if (google_api_init() == ESP_OK)
        {
            google_api_params_init();

            // https://github.com/DaveGamble/cJSON#data-structure
            cJSON *root = cJSON_CreateObject();
            cJSON *rows = cJSON_AddArrayToObject(root, "values");

            cJSON *cols = cJSON_CreateArray();
            strftime(strftime_buf, sizeof(strftime_buf), "%Y/%m/%d %T", &timeinfo);
            cJSON_AddItemToArray(cols, cJSON_CreateNumber(boot_count));
            cJSON_AddItemToArray(cols, cJSON_CreateString(strftime_buf));
            // snprintf(val_buf, 9, "%.03f", battery);
            // cJSON_AddItemToArray(cols, cJSON_CreateString(val_buf));
            cJSON_AddItemToArray(cols, cJSON_CreateNumber(battery));
            cJSON_AddItemToArray(cols, cJSON_CreateNumber(adt7410_temp));
            cJSON_AddItemToArray(cols, cJSON_CreateNumber(shtc3_temp));
            cJSON_AddItemToArray(cols, cJSON_CreateNumber(shtc3_humi));

            cJSON_AddItemToArray(rows, cols);

            // cJSON *cols = cJSON_CreateArray();
            // cJSON_AddItemToArray(cols, cJSON_CreateNumber(boot_count));
            // cJSON_AddItemToArray(cols, cJSON_CreateString(strftime_buf));
            // cJSON_AddItemToArray(cols, cJSON_CreateNumber(fix3(battery)));
            // cJSON_AddItemToArray(cols, cJSON_CreateNumber(fix3(adt7410_temp)));
            // cJSON_AddItemToArray(cols, cJSON_CreateNumber(fix3(shtc3_temp)));
            // cJSON_AddItemToArray(cols, cJSON_CreateNumber(fix3(shtc3_humi)));

            // cJSON *rows = cJSON_CreateArray();
            // cJSON_AddItemToArray(rows, cols);

            // cJSON *root = cJSON_CreateObject();
            // cJSON_AddItemToObject(root, "values", rows);

            const char *post_data = cJSON_PrintUnformatted(root);
            ESP_LOGI(TAG, "Update values: %s", post_data);

            google_shreadsheet_append("1O7YsMmfNes7ccYWbo50w7YrjcmLU4pmbenio-fqGKWo", "eneloop!A1", post_data);

            cJSON_Delete(root);
            cJSON_free(post_data);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to init Google API");
        }

        // update daily lowestlowest temp.
        // highest in April to November.
        if (timeinfo.tm_hour == 0)
        {
            // reset updated tweet
            tweeted_lowest_temp = false;
            tweeted_highest_temp = false;
        }

        if (!tweeted_lowest_temp && (timeinfo.tm_hour == TWEET_LOWEST_AT))
        {
            snprintf(tweet, TWEET_BUF_SIZE,
                     "%d月%d日\n今朝の最低室温: %.01f℃\n"
                     "現在の電圧: %.02fV\n"
                     "起動%d回目 #ESP32",
                     timeinfo.tm_mon + 1,
                     timeinfo.tm_mday,
                     lowest_temp,
                     battery,
                     boot_count);
            lowest_temp = 100;
            tweeted_lowest_temp = true;
        }
        else if (!tweeted_highest_temp && (timeinfo.tm_hour == TWEET_HIGHEST_AT))
        {
            snprintf(tweet, TWEET_BUF_SIZE,
                     "%d月%d日\n今日の最高室温: %.01f℃\n"
                     "現在の電圧: %.02fV\n"
                     "起動%d回目 #ESP32",
                     timeinfo.tm_mon + 1,
                     timeinfo.tm_mday,
                     highest_temp,
                     battery,
                     boot_count);

            highest_temp = 0;
            tweeted_highest_temp = true;
        }

        // cancel update status untile collecting data over 24 hours.
        if (boot_count == 1)
        {
            snprintf(tweet, TWEET_BUF_SIZE,
                     "起動: %d月%d日 %d時%d分\n現在から24時間は、データ収集のみに専念します。その後、定時のTweetを開始します。",
                     timeinfo.tm_mon + 1,
                     timeinfo.tm_mday,
                     timeinfo.tm_hour,
                     timeinfo.tm_min);
        }
        else if ((now - boot_at) < (24 * 3600))
            tweet[0] = '\0';

        if (tweet[0] != '\0')
        {
            printf("Update status: %s\n", tweet);

            if (twitter2_init() == ESP_OK)
            {
                twitter2_api_params_init();
                twitter2_api_param("text", tweet);
                twitter2_update_status();
            }
            else
            {
                ESP_LOGE(TAG, "Failed to init Twitter");
            }
        }

        // Say Good bye.
        if (mean_battery < 4.0)
        {
            snprintf(tweet, TWEET_BUF_SIZE,
                     "Battery level: %.02fV\nBattery level reached lower limit. Move to deep sleep mode.\nGood bye.",
                     mean_battery);
            if (twitter2_init() == ESP_OK)
            {
                twitter2_api_params_init();
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

    // Termination voltaege for Ni-MH is 1.0V and 4.0V with 4 batteries.
    // The termination volatge of alkari battery is 0.8V and 3.2V with 4 batteries.
    // But this is lower than required volatage to make 3.3V.
    // So, I will make this stop working when that also reached to 4.0V.
    if (mean_battery < 4.0)
    {
        // never wakeup.
        ESP_LOGW(TAG, "Entering deep sleep. Never wakeup because low battery: %.01f", battery);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    }
    else
    {
        ESP_LOGI(TAG, "Entering deep sleep for %d min.", CONFIG_SLEEP_LENGTH);
        esp_sleep_enable_timer_wakeup(1000000LL * CONFIG_SLEEP_LENGTH * 60);
        // esp_deep_sleep(1000000LL * SLEEP_SEC);
    }

    // Keep GPIO state. GPIO changes in deep sleep.
    // But this consumes battery while deep sleep.
    gpio_hold_en(CONFIG_LED_PIN);
    gpio_deep_sleep_hold_en();

    esp_deep_sleep_start();
}
