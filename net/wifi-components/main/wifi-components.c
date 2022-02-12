#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ns_wifi.h"
#include "ns_http.h"

static const char *TAG = "main";

void app_main(void)
{
    nvs_init();
    config_gpio();
    init_spiffs();

    if (wifi_init() | pressed_triger())
    {
        ESP_LOGW(TAG, "No AP info in NVS or Pressed triger button");
        wifi_ap_select_mode();
    }
    ESP_LOGI(TAG, "Try to connect");
    wifi_connect();

    ESP_LOGI(TAG, "Wait connection");
    if (wifi_wait_connection())
    {
        ns_http_get("https://www.yahoo.co.jp");
        vTaskDelay(pdMS_TO_TICKS(1000));
        ns_http_get("https://www.google.com/");

        wifi_disconnect();
        ESP_LOGI(TAG, "Disconnected");
    }
    else
    {
        ESP_LOGW(TAG, "Failed to connect");
    }
}
