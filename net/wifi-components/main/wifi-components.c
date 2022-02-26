#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ns_wifi.h"
#include "ns_http.h"

static const char *TAG = "main";

void app_main(void)
{
    HTTP_CONTENT content = {
        .size = 0,
        .body = NULL,
    };

    ESP_LOGI(TAG, "Init Wifi");
    wifi_init();
    ESP_LOGI(TAG, "Try to connect");
    wifi_connect();

    ESP_LOGI(TAG, "Wait connection");
    if (wifi_wait_connection())
    {
        ns_http_get("https://www.yahoo.co.jp", 0, &content);
        if (content.body != NULL)
        {
            free(content.body);
            content.body = NULL;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));

        ns_http_get("https://www.google.com/", 0, NULL);

        wifi_disconnect();
        ESP_LOGI(TAG, "Disconnected");
    }
    else
    {
        ESP_LOGW(TAG, "Failed to connect");
    }
}
