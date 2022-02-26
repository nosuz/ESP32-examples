#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"

#include "ns_http.h"

#define BUFFER_BLOCK_SIZE (5 * 1024)

static const char *TAG = "http";

// const char *pem_nosuz asm("_binary_wwww_nosuz_jp_pem_start");

esp_err_t ns_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        // ESP_LOGI(
        //     TAG,
        //     "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
        //     evt->header_key,
        //     evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        // Can handle both pages having Content-Length and chunked pages.
        // ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (evt->user_data == NULL)
            break;
        HTTP_CONTENT *content = evt->user_data;
        int64_t new_content_size = content->size + evt->data_len;
        // check the allocated memory size and expand it if needed.
        int cur_block_size = (content->size + 1) / BUFFER_BLOCK_SIZE + 1;
        int new_block_size = (new_content_size + 1) / BUFFER_BLOCK_SIZE + 1;
        if (cur_block_size < new_block_size)
        {
            char *tmp = (char *)realloc(content->body, new_block_size * BUFFER_BLOCK_SIZE);
            if (tmp != NULL)
            {
                // ESP_LOGI(TAG, "Expanded to %d blocks.", new_block_size);
                content->body = tmp;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to realloc to %d blocks.", new_block_size);
                break;
            }
        }

        memcpy(content->body + content->size, evt->data, evt->data_len);
        content->body[new_content_size] = '\0';

        content->size = new_content_size;

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
        if (err != ESP_OK)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        break;
    }
    return ESP_OK;
}

esp_err_t ns_http_get(char *url, HTTP_CONTENT *content, unsigned int timeout_sec)
{
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = ns_http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = content,
    };
    if (timeout_sec > 0)
        config.timeout_ms = timeout_sec * 1000;

    if (content != NULL)
    {
        content->size = 0;
        if (content->body != NULL)
        {
            free(content->body);
            // content->body = NULL;
        }
        content->body = malloc(BUFFER_BLOCK_SIZE);
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int64_t content_length = esp_http_client_get_content_length(client);
        ESP_LOGI(
            TAG,
            "HTTPS Status = %d, content_length = %lld",
            esp_http_client_get_status_code(client),
            content_length);

        if (content != NULL)
            ESP_LOGI(TAG, "Content size: %lld", content->size);
    }
    else
    {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);

    return err;
}
