#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_sleep.h"

#include "ns_adc.h"
#include "ns_adt7410.h"
#include "ns_shtc3.h"

#include "ns_wifi.h"
#include "ns_ambient.h"

static const char *TAG = "main";

RTC_DATA_ATTR static int boot_count = 0;

void app_main(void)
{
    uint32_t voltage = 0;
    float battery = 5;
    float adt7410_temp = 0;
    float shtc3_temp = 0;
    float shtc3_humi = 0;

    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    init_adc();
    init_i2c_master();

    read_adc_voltage(&voltage);
    battery = (float)voltage * 3 / 1000; // (100k + 200k) / 100k = 3
    printf("Battery: %0.2f V\n", battery);

    adt7410_read_temp(&adt7410_temp);
    shtc3_read_sensor(&shtc3_temp, &shtc3_humi);

    ESP_LOGI(TAG, "Init WiFi");
    wifi_init();
    ESP_LOGI(TAG, "Try to connect");
    wifi_connect();

    ESP_LOGI(TAG, "Wait connection");
    if (wifi_wait_connection())
    {
        printf("室温1: %.01f℃, 室温2: %.01f℃, 湿度: %.0f%% "
               "電圧: %.02fV "
               "起動%d回目",
               adt7410_temp,
               shtc3_temp, shtc3_humi,
               battery,
               boot_count);

        ambient_set(1, adt7410_temp);
        ambient_set(2, shtc3_temp);
        ambient_set(3, shtc3_humi);
        ambient_set(4, battery);

        ambient_send();
    }
    else
    {
        ESP_LOGW(TAG, "Failed to connect");
    }

    wifi_disconnect();

    // Termination voltaege for Ni-MH is 1.0V and 4.0v for 4 batteries.
    // Termination voltaege for Alkaline is 0.8V and 3.2v for 4 batteries. But some space are required for power supply.
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
