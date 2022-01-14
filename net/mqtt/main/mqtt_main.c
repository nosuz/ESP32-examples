/* This code is from ESP-IDF example.

   This code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"

#include "gpio.h"
#include "nvs.h"
#include "wifi.h"
#include "spi.h"
#include "ADT7310.h"
#include "mqtt.h"

#define SLEEP_SEC (60)

static const char *TAG = "wifi_main";

RTC_DATA_ATTR static int boot_count = 0;

void app_main(void)
{
    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    nvs_init();
    config_gpio();

    // Setup SPI and device.
    init_spi();
    attach_adt7310();
    if (boot_count == 0)
    {
        // wait filling current temp into registor.
        vTaskDelay(pdMS_TO_TICKS(1000)); // wait 1 sec.
    }

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
        init_mqtt();

        vTaskDelay(pdMS_TO_TICKS(500)); // need to wait 1 sec.
        float temp = adt7310_read_temp();
        printf("Temp: %.1fC\n", temp);
        char buf[7];
        snprintf(buf, 7, "%.01f", temp);
        mqtt_publish("esp32/spi", buf, 0, 0);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to connect");
        wifi_disconnect();
    }

    ESP_LOGI(TAG, "Entering deep sleep for %d sec.", SLEEP_SEC);
    esp_deep_sleep(1000000LL * SLEEP_SEC);
}
