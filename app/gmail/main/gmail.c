#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "cJSON.h"

#include "ns_wifi.h"
#include "ns_google_api.h"

#define STRFTIME_SIZE 64

#define HEADER "To: nosuzuki@postcard.st\nSubject: test =?utf-8?B?5pel5pys6Kqe44Gu44Oh44O844Or?=\nContent-Type: text/plain\n"
#define BODY "This is a test message.\n日本語のメールはOK?\n\n"

static const char *TAG = "main";

void app_main(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[STRFTIME_SIZE];
    char *message;
    // +1 for separator between HEADER and BODY, +2 for "\n\0"
    message = malloc(strlen(HEADER) + strlen("DATE: ") + STRFTIME_SIZE + strlen(BODY) + STRFTIME_SIZE + 3);
    if (message == NULL)
        ESP_LOGE(TAG, "Failed malloc for message");

    ESP_LOGI(TAG, "Init Wifi");
    wifi_init();
    ESP_LOGI(TAG, "Try to connect");
    wifi_connect();

    ESP_LOGI(TAG, "Wait connection");
    if (wifi_wait_connection())
    {
        if (google_api_init() == ESP_OK)
        {
            google_api_params_init();

            time(&now);
            localtime_r(&now, &timeinfo);

            gmaile_formatdate(strftime_buf, sizeof(strftime_buf), &timeinfo);
            ESP_LOGI(TAG, "Local date/time: %s", strftime_buf);

            strcpy(message, HEADER);
            // Date field is overwriten by Google. So does not need to set date field.
            strcat(message, "Date: ");
            strcat(message, strftime_buf);
            strcat(message, "\n\n");
            strcat(message, BODY);
            strcat(message, strftime_buf);
            strcat(message, "\n");
            ESP_LOGI(TAG, "Mail: %s", message);

            gmail_create_draft(message);

            vTaskDelay(pdMS_TO_TICKS(2 * 1000));

            time(&now);
            localtime_r(&now, &timeinfo);

            gmaile_formatdate(strftime_buf, sizeof(strftime_buf), &timeinfo);
            strcpy(message, HEADER);
            strcat(message, "Date: ");
            strcat(message, strftime_buf);
            strcat(message, "\n\n");
            strcat(message, BODY);

            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            ESP_LOGI(TAG, "Local date/time: %s", strftime_buf);
            strcat(message, strftime_buf);
            strcat(message, "\n");
            ESP_LOGI(TAG, "Mail: %s", message);

            gmail_send(message);
        }
    }
    else
    {
        ESP_LOGW(TAG, "Failed to connect");
    }

    wifi_disconnect();

    free(message);
}
