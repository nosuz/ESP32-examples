#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"

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

esp_http_client_config_t ns_http_default_config(esp_http_client_method_t method, const char *url)
{
    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .event_handler = ns_http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = NULL,
    };

    return config;
}

HTTP_STRUCT *ns_http_init(esp_http_client_method_t method, const char *url)
{
    HTTP_STRUCT *http = malloc(sizeof(HTTP_STRUCT));
    http->config = ns_http_default_config(method, url);
    http->header = NULL;
    http->last_header = NULL;
    http->post_data = NULL;

    return http;
}

HTTP_STRUCT *ns_http_get(const char *url)
{
    HTTP_STRUCT *http = ns_http_init(HTTP_METHOD_GET, url);

    return http;
}

HTTP_STRUCT *ns_http_post(const char *url, const char *content_type, const char *data, unsigned int data_size)
{
    HTTP_STRUCT *http = ns_http_init(HTTP_METHOD_POST, url);
    http->post_data = data;
    http->data_size = data_size;
    ns_http_set_header(http, "Content-Type", content_type);

    return http;
}

void ns_http_set_timeout(HTTP_STRUCT *http, unsigned int timeout_sec)
{
    esp_http_client_config_t *config;
    config = &http->config;
    config->timeout_ms = timeout_sec * 1000;
}

void ns_http_auth_basic(HTTP_STRUCT *http, const char *user_name, const char *password)
{
    ESP_LOGI(TAG, "Set AUTH BASIC");
    esp_http_client_config_t *config;
    config = &http->config;
    config->auth_type = HTTP_AUTH_TYPE_BASIC;
    config->username = user_name;
    config->password = password;
}

void ns_http_set_header(HTTP_STRUCT *http, const char *key, const char *value)
{
    HTTP_HEADER *new_header;
    new_header = malloc(sizeof(HTTP_HEADER));
    new_header->next = NULL;
    new_header->key = malloc(strlen(key) + 1);
    new_header->value = malloc(strlen(value) + 1);
    strcpy(new_header->key, key);
    strcpy(new_header->value, value);

    if (http->header == NULL)
    {
        http->header = new_header;
    }
    else
    {
        http->last_header->next = new_header;
    }
    http->last_header = new_header;
}

esp_err_t ns_http_send(HTTP_STRUCT *http, HTTP_CONTENT *content)
{
    esp_http_client_config_t *config;
    config = &http->config;
    config->user_data = content;
    // ESP_LOGI(TAG, "URL: %s", config->url);
    // ESP_LOGI(TAG, "Auth method(%d)", config->auth_type);
    // if (config->auth_type != 0)
    //     ESP_LOGI(TAG, "Auth method: %s:%s", config->username, config->password);

    // send request
    if (content != NULL)
    {
        content->size = 0;
        if (content->body != NULL)
        {
            free(content->body);
            content->body = NULL;
        }
        content->body = malloc(BUFFER_BLOCK_SIZE);
    }

    esp_http_client_handle_t client = esp_http_client_init(config);
    // set header
    HTTP_HEADER *header;
    header = http->header;
    while (header != NULL)
    {
        esp_http_client_set_header(client, header->key, header->value);
        // ESP_LOGI(TAG, "Header: %s -> %s", header->key, header->value);
        header = header->next;
    }
    if (http->post_data != NULL)
        esp_http_client_set_post_field(client, http->post_data, http->data_size);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
        // if (content != NULL)
        //     printf("%s\n", content.body)
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    // cleanup
    esp_http_client_cleanup(client);
    ns_http_free(http);

    return err;
}

void ns_http_free(HTTP_STRUCT *http)
{
    HTTP_HEADER *next_header;
    HTTP_HEADER *header;
    header = http->header;
    while (header != NULL)
    {
        free(header->key);
        free(header->value);
        next_header = header->next;
        free(header);

        header = next_header;
    }

    free(http);
    http = NULL;
}

void ns_http_auth_bearer(HTTP_STRUCT *http, const char *access_token)
{
    char *auth_value;
    auth_value = malloc(strlen("Bearer ") + strlen(access_token) + 1);
    strcpy(auth_value, "Bearer ");
    strcat(auth_value, access_token);

    ns_http_set_header(http, "Authorization", auth_value);
    free(auth_value);
}

HTTP_CONTENT ns_http_default_content(void)
{
    HTTP_CONTENT content = {
        .size = 0,
        .body = NULL,
    };

    return content;
}

void ns_http_content_cleanup(HTTP_CONTENT *content)
{
    if (content != NULL)
    {
        free(content->body);
        content->body = NULL;
        content->size = 0;
    }
}
