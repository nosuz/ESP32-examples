#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "cJSON.h"

#include "gpio.h"
#include "nvs.h"
#include "wifi.h"
#include "spi.h"
#include "adt7310.h"
#include "mqtt.h"
#include "sntp.h"

#define SLEEP_SEC (10 * 60) // sleep 10 min.

static const char *TAG = "wifi_main";

RTC_DATA_ATTR static int boot_count = 0;
RTC_DATA_ATTR static time_t last_synced = 0;

time_t now;
struct tm timeinfo;
char strftime_buf[64];

void app_main(void)
{
    float temp;

    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    nvs_init();
    config_gpio();

    // setenv("TZ", TIME_ZONE, 1);
    setenv("TZ", "JST-9", 1);
    tzset();
    init_sntp();

    // Setup SPI and device.
    init_spi();
    attach_adt7310();

    temp = adt7310_read_temp();
    printf("Temp: %.1fC\n", temp);

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
        ESP_LOGI(TAG, "Connected AP");

        // Get systemtime into `now` and set local time in `timeinfo`
        time(&now);
        localtime_r(&now, &timeinfo);
        if (((now - last_synced) > (3600 * 2)) || (timeinfo.tm_year < (2016 - 1900)))
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

        init_mqtt();

        cJSON *root = cJSON_CreateObject();
        strftime(strftime_buf, sizeof(strftime_buf), "%FT%T", &timeinfo);
        cJSON_AddStringToObject(root, "time", strftime_buf);
        cJSON_AddNumberToObject(root, "boot", boot_count);
        cJSON_AddNumberToObject(root, "temp", temp);

        // char *json = cJSON_Print(root);
        // printf(cJSON_Print(root));
        char *json = cJSON_PrintUnformatted(root);
        mqtt_publish("esp32/spi", json, 0, 0);

        mqtt_stop();
        cJSON_Delete(root);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to connect");
    }
    wifi_disconnect();

    ESP_LOGI(TAG, "Entering deep sleep for %d sec.", SLEEP_SEC);
    esp_deep_sleep(1000000LL * SLEEP_SEC);
}
