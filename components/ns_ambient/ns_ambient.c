#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"

#include "ns_ambient.h"

#define BUFFER_BLOCK_SIZE (1024)

#define MAX_DATA 8
#define URL_BASE "http://ambidata.io/api/v2/channels/" CONFIG_AMBIENT_CHANNEL_ID
// #define SEND_NODE "/dataarray"
#define SEND_NODE "/data"

static const char *TAG = "ambient";

typedef struct
{
    bool valid;
    float value;
} Ambient;

typedef struct
{
    int64_t size;
    char *body;
} content_struct;

Ambient data[MAX_DATA];
bool initilized_data = false;

esp_err_t http_event_handler(esp_http_client_event_t *evt)
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
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (evt->user_data == NULL)
            break;
        content_struct *content = evt->user_data;
        int64_t new_content_size = content->size + evt->data_len;
        if (content->body != NULL)
        {
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
        }
        content->size = new_content_size;

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        break;
    default:
        ESP_LOGI(TAG, "Unknown event id: %d", evt->event_id);
    }
    return ESP_OK;
}

void ambient_init(void)
{
    for (int i = 0; i < MAX_DATA; i++)
    {
        data[i].valid = false;
    }
    initilized_data = true;
}

esp_err_t ambient_set(int d, float value)
{
    // d: 1 to 8
    if (d > MAX_DATA || d <= 0)
        return ESP_ERR_INVALID_ARG;

    if (!initilized_data)
        ambient_init();

    d--;
    data[d].valid = true;
    data[d].value = value;

    return ESP_OK;
}

esp_err_t ambient_send(void)
{
    if (!initilized_data)
        return ESP_FAIL;

    // construct URL
    char *url = malloc(strlen(URL_BASE) + strlen(SEND_NODE) + 1);
    strcpy(url, URL_BASE);
    strcat(url, SEND_NODE);

    // construct JSON data
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "writeKey", CONFIG_AMBIENT_WRITE_KEY);

    // set data
    cJSON *array = cJSON_CreateArray();
    char *index = "d?";
    // for (int i = 0; i < MAX_DATA; i++)
    // {
    //     if (data[i].valid)
    //     {
    //         // sprintf(index, "d%d", i);
    //         index[1] = '1' + i;
    //         cJSON *d = cJSON_CreateObject();
    //         cJSON_AddNumberToObject(d, index, data[i].value);
    //         cJSON_AddItemToArray(array, d);
    //     }
    // }
    // cJSON_AddItemToObject(root, "data", array);
    for (int i = 0; i < MAX_DATA; i++)
    {
        if (data[i].valid)
        {
            // sprintf(index, "d%d", i);
            index[1] = '1' + i;
            cJSON_AddNumberToObject(root, index, data[i].value);
        }
    }

    const char *json_data = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG, "%s", json_data);

    content_struct content = {
        .size = 0,
        .body = malloc(sizeof(char) * BUFFER_BLOCK_SIZE),
    };

    esp_http_client_config_t config = {
        .url = url,
        // .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = &content,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // POST
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_data, strlen(json_data));
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
        // printf("%s\n", content.body);
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    // cleanup
    esp_http_client_cleanup(client);
    free(content.body);
    free(json_data);
    cJSON_Delete(root);
    free(url);

    return ESP_OK;
}
