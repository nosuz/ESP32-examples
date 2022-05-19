#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_sleep.h"
#include "cJSON.h"

#include "ns_http.h"
#include "ns_adc.h"
#include "ns_adt7410.h"
#include "ns_shtc3.h"

#include "ns_wifi.h"

#define VOLTS_HIST 3
#define MAX_UNEXPECTED_REBOOT 2

// URL for Google Spreadsheet HTTP access
static const char *url = CONFIG_SPREADSHEET_API_URL;

static const char *TAG = "main";

RTC_DATA_ATTR static int boot_count = 0;
RTC_DATA_ATTR static float up_time = 0;
RTC_DATA_ATTR static time_t last_boot = 0;
RTC_DATA_ATTR static int unexpected_reboot = 0;

RTC_DATA_ATTR static float volts[VOLTS_HIST] = {0};

// https://esp32.com/viewtopic.php?t=5288
float get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (float)(tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL)) / 1000.0;
}

void app_main(void)
{
    float boot_at = get_time();

    uint32_t voltage = 0;
    float battery = 0;
    float mean_battery = 0;
    float adt7410_temp = 0;
    float shtc3_temp = 0;
    float shtc3_humi = 0;

    uint8_t base_mac_addr[6] = {0};
    char mac_addr[18];

    time_t now;
    time(&now);
    if ((boot_count > 0) && ((now - last_boot) < 180)) // expect the reboot interval is at least 3 min.
    {
        // unexpected reboot
        unexpected_reboot++;
        ESP_LOGE(TAG, "Unexpected reboot: %d", unexpected_reboot);
        if (unexpected_reboot > MAX_UNEXPECTED_REBOOT)
        {
            ESP_LOGE(TAG, "Give up working and go into deep sleep");
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
            esp_deep_sleep_start();
        }
    }
    else
    {
        unexpected_reboot = 0;
    }
    last_boot = now;

    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    esp_efuse_mac_get_default(base_mac_addr);
    snprintf(mac_addr, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             base_mac_addr[0], base_mac_addr[1], base_mac_addr[2], base_mac_addr[3], base_mac_addr[4], base_mac_addr[5]);
    ESP_LOGI(TAG, "MAC Address: %s", mac_addr);

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

    adt7410_set_high_resoluton();
    adt7410_read_temp(&adt7410_temp);
    shtc3_read_sensor(&shtc3_temp, &shtc3_humi);

    ESP_LOGI(TAG, "Init Wifi");
    wifi_init();
    ESP_LOGI(TAG, "Try to connect");
    wifi_connect();

    ESP_LOGI(TAG, "Wait connection");
    if (wifi_wait_connection())
    {
        ESP_LOGI(TAG, "室温1: %.01f℃, 室温2: %.01f℃, 湿度: %.0f%% "
                      "電圧: %.02fV "
                      "起動%d回目",
                 adt7410_temp,
                 shtc3_temp, shtc3_humi,
                 battery,
                 boot_count);

        // https://github.com/DaveGamble/cJSON#data-structure
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "secret", CONFIG_SECRET_TOKEN);

        cJSON *data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "id", mac_addr);
        cJSON_AddNumberToObject(data, "bat", battery);
        cJSON_AddNumberToObject(data, "t_ad", adt7410_temp);
        cJSON_AddNumberToObject(data, "t_sh", shtc3_temp);
        cJSON_AddNumberToObject(data, "h_sh", shtc3_humi);
        cJSON_AddNumberToObject(data, "cnt", boot_count);
        cJSON_AddItemToObject(data, "fin", mean_battery < 4.0 ? cJSON_CreateTrue() : cJSON_CreateFalse());
        cJSON_AddNumberToObject(data, "up", up_time);

        cJSON_AddItemToObject(root, "data", data);

        char *post_data = cJSON_PrintUnformatted(root);
        ESP_LOGI(TAG, "Post values: %s", post_data);

        // POST JSON to Google Spreadsheet
        HTTP_STRUCT *http = ns_http_post(url, "application/json", post_data, strlen(post_data));
        ns_http_send(http, NULL);

        cJSON_Delete(root);
        cJSON_free(post_data);
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

        // Keep GPIO state. GPIO changes in deep sleep.
        // But this consumes battery while deep sleep.
        gpio_hold_en(CONFIG_LED_PIN);
        gpio_deep_sleep_hold_en();
    }

    up_time = get_time() - boot_at;

    esp_deep_sleep_start();
}
