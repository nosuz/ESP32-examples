#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"

#include "ns_sntp.h"
#include "ns_twitter2.h"

#define MAX_PARAMS 16
#define BUFFER_BLOCK_SIZE (1024)

#define CLIENT_ID CONFIG_TWITTER_CLIENT_ID
#define CLIENT_SECRET CONFIG_TWITTER_CLIENT_SECRET
#define REFRESH_TOKEN CONFIG_TWITTER_REFHRESH_TOKEN

#define KEY_CLIENT_ID "client_id"
#define KEY_CLIENT_SECRET "client_sec"
#define KEY_INIT_REFRESH_TOKEN "init_refresh"
#define KEY_REFRESH_TOKEN "refresh_token"
#define KEY_ACCESS_TOKEN "access_token"
#define KEY_LAST_REFRESHED "refreshed_at"

// #define DEBUG

static const char *TAG = "twitter2";

#ifdef CONFIG_TWITTER_PRIVATE_CLIENT
char *client_secret;
#endif

char *client_id;
char *refresh_token;
char *access_token;
nvs_handle_t twitter_nvs_handle;

typedef struct
{
    char *key;
    char *value;
} TWITTER_API_PARAM;

int param_index = 0;
TWITTER_API_PARAM api_params[MAX_PARAMS];

typedef struct content_struct
{
    int64_t size;
    char *body;
} content_struct;

esp_err_t http_twitter_event_handler(esp_http_client_event_t *evt)
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

esp_err_t twitter2_refresh_token(void)
{
    const char *url = "https://api.twitter.com/2/oauth2/token";

    char *post_data;
    post_data = malloc(strlen("grant_type=refresh_token") + strlen("refresh_token=") + strlen(refresh_token) + strlen("client_id=") + strlen(client_id) + 3);
    strcpy(post_data, "grant_type=refresh_token&refresh_token=");
    strcat(post_data, refresh_token);
    strcat(post_data, "&client_id=");
    strcat(post_data, client_id);
    // ESP_LOGI(TAG, "Refresh Token param: %s", post_data);

    content_struct content = {
        .size = 0,
        .body = malloc(sizeof(char) * BUFFER_BLOCK_SIZE),
    };

    esp_http_client_config_t config = {
        .url = url,
        // .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = http_twitter_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = &content,
        .method = HTTP_METHOD_POST,
#ifdef CONFIG_TWITTER_PRIVATE_CLIENT
        .auth_type = HTTP_AUTH_TYPE_BASIC,
        .username = client_id,
        .password = client_secret,
#endif
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // POST
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
        // printf("%s\n", content.body);

        cJSON *new_access_token = NULL;
        cJSON *new_refresh_token = NULL;
        cJSON *api_error = NULL;
        cJSON *res_json = cJSON_Parse(content.body);
        if (res_json == NULL)
        {
            ESP_LOGE(TAG, "Wrong Response: %s", content.body);
            err = ESP_FAIL;
        }
        else
        {
            api_error = cJSON_GetObjectItemCaseSensitive(res_json, "error");
            if (cJSON_IsString(api_error) && (api_error->valuestring != NULL))
            {
                ESP_LOGE(TAG, "Refresh Token Error: %s", content.body);
                err = ESP_FAIL;
            }
            else
            {
                ESP_LOGI(TAG, "Refreshed Token: Got new Access Token.");
                new_access_token = cJSON_GetObjectItemCaseSensitive(res_json, "access_token");
                if (cJSON_IsString(new_access_token) && (new_access_token->valuestring != NULL))
                {
                    if (access_token != NULL)
                        free(access_token);
                    access_token = malloc(strlen(new_access_token->valuestring) + 1);
                    strcpy(access_token, new_access_token->valuestring);
                    // ESP_LOGI(TAG, "Refreshed Access Token: %s", access_token);
                }
                new_refresh_token = cJSON_GetObjectItemCaseSensitive(res_json, "refresh_token");
                if (cJSON_IsString(new_refresh_token) && (new_refresh_token->valuestring != NULL))
                {
                    if (refresh_token != NULL)
                        free(refresh_token);
                    refresh_token = malloc(strlen(new_refresh_token->valuestring) + 1);
                    strcpy(refresh_token, new_refresh_token->valuestring);
                    // ESP_LOGI(TAG, "Refreshed Refresh Token: %s", refresh_token);
                }
            }

            cJSON_Delete(res_json);
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(post_data);
    free(content.body);

    return err;
}

esp_err_t twitter2_init(void)
{
    init_sntp(3600);
    // update system clock
    start_sntp();

    esp_err_t err = nvs_open("twitter2", NVS_READWRITE, &twitter_nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Opening NVS error");
        return ESP_FAIL;
    }

    bool new_params = false;
    size_t required_size;
    uint32_t last_update;
    err = nvs_get_u32(twitter_nvs_handle, KEY_LAST_REFRESHED, &last_update);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "No params in NVS");
        new_params = true;
    }
    else if (client_id == NULL)
    {
        ESP_LOGI(TAG, "Validate Twitter params");

        nvs_get_str(twitter_nvs_handle, KEY_CLIENT_ID, NULL, &required_size);
        client_id = malloc(required_size);
        nvs_get_str(twitter_nvs_handle, KEY_CLIENT_ID, client_id, &required_size);
        if (strcmp(client_id, CLIENT_ID) != 0)
            new_params = true;

        nvs_get_str(twitter_nvs_handle, KEY_INIT_REFRESH_TOKEN, NULL, &required_size);
        char *init_refresh_token;
        init_refresh_token = malloc(required_size);
        nvs_get_str(twitter_nvs_handle, KEY_INIT_REFRESH_TOKEN, init_refresh_token, &required_size);
        if (strcmp(init_refresh_token, REFRESH_TOKEN) != 0)
            new_params = true;
        free(init_refresh_token);
    }

    if (new_params)
    {
        // update NVS
        ESP_LOGI(TAG, "Update Twitter params in NVS");

        if (client_id != NULL)
            free(client_id);
        client_id = malloc(strlen(CLIENT_ID) + 1);
        strcpy(client_id, CLIENT_ID);
        nvs_set_str(twitter_nvs_handle, KEY_CLIENT_ID, client_id);

#ifdef CONFIG_TWITTER_PRIVATE_CLIENT
        client_secret = malloc(strlen(CLIENT_SECRET) + 1);
        strcpy(client_secret, CLIENT_SECRET);
        nvs_set_str(twitter_nvs_handle, KEY_CLIENT_SECRET, client_secret);
#else
        nvs_set_str(twitter_nvs_handle, KEY_CLIENT_SECRET, "");
#endif

        nvs_set_str(twitter_nvs_handle, KEY_INIT_REFRESH_TOKEN, REFRESH_TOKEN);

        refresh_token = malloc(strlen(REFRESH_TOKEN) + 1);
        strcpy(refresh_token, REFRESH_TOKEN);
    }
    else
    {
        // get from NVS
        ESP_LOGI(TAG, "Use Twitter params in NVS");

        if (client_id == NULL)
        {
            nvs_get_str(twitter_nvs_handle, KEY_CLIENT_ID, NULL, &required_size);
            client_id = malloc(required_size);
            nvs_get_str(twitter_nvs_handle, KEY_CLIENT_ID, client_id, &required_size);
        }
#ifdef CONFIG_TWITTER_PRIVATE_CLIENT
        nvs_get_str(twitter_nvs_handle, KEY_CLIENT_SECRET, NULL, &required_size);
        client_secret = malloc(required_size);
        nvs_get_str(twitter_nvs_handle, KEY_CLIENT_SECRET, client_secret, &required_size);
#endif

        nvs_get_str(twitter_nvs_handle, KEY_REFRESH_TOKEN, NULL, &required_size);
        refresh_token = malloc(required_size);
        nvs_get_str(twitter_nvs_handle, KEY_REFRESH_TOKEN, refresh_token, &required_size);

        nvs_get_str(twitter_nvs_handle, KEY_ACCESS_TOKEN, NULL, &required_size);
        access_token = malloc(required_size);
        nvs_get_str(twitter_nvs_handle, KEY_ACCESS_TOKEN, access_token, &required_size);
    }

    time_t now;
    time(&now);
    if (new_params || (now > (last_update + CONFIG_TWITTER_TOKEN_EXPIRE_TIME - 180)))
    {
        ESP_LOGI(TAG, "Refresh OAuth token");
        // expired, refresh token.
        err = twitter2_refresh_token();
        if (err == ESP_OK)
        {
            new_params = true;

            nvs_set_u32(twitter_nvs_handle, KEY_LAST_REFRESHED, now);
            nvs_set_str(twitter_nvs_handle, KEY_ACCESS_TOKEN, access_token);
            nvs_set_str(twitter_nvs_handle, KEY_REFRESH_TOKEN, refresh_token);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to refresh TOKEN");
            err = ESP_FAIL;
            goto exit_refresh_token;
        }
    }
    ESP_LOGI(TAG, "Client ID: %s", client_id);

    if (new_params)
    {
        // commit NVS
        err = nvs_commit(twitter_nvs_handle);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "NVS commit: OK");
        }
        else
        {
            ESP_LOGE(TAG, "NVS commit: Failed");
            err = ESP_FAIL;
            goto exit_refresh_token;
        }
    }

exit_refresh_token:
    nvs_close(twitter_nvs_handle);
    return err;
}

void twitter2_api_init(void)
{
    for (int i = 0; i < param_index; i++)
    {
        if (api_params[i].key != NULL)
        {
            free(api_params[i].key);
            api_params[i].key = NULL;
        }

        if (api_params[i].value != NULL)
        {
            free(api_params[i].value);
            api_params[i].value = NULL;
        }
    }

    param_index = 0;
}

void twitter2_api_param(char *key, char *value)
{
    // ESP_LOGI(TAG, "%s: %s", key, value);
    if (param_index < MAX_PARAMS)
    {
        api_params[param_index].key = malloc(strlen(key) + 1);
        strcpy(api_params[param_index].key, key);
        api_params[param_index]
            .value = malloc(strlen(value) + 1);
        strcpy(api_params[param_index].value, value);
        param_index++;
    }
    else
    {
        ESP_LOGW(TAG, "No space for API params.");
    }
}

void twitter2_update_status(void)
{
    const char *url = "https://api.twitter.com/2/tweets";

    char *auth_header;
    auth_header = malloc(strlen("Bearer ") + strlen(access_token) + 1);
    strcpy(auth_header, "Bearer ");
    strcat(auth_header, access_token);

    cJSON *root = cJSON_CreateObject();
    for (int i = 0; i < param_index; i++)
    {
        cJSON_AddStringToObject(root, api_params[i].key, api_params[i].value);
    }
    const char *post_data = cJSON_PrintUnformatted(root);
    // printf("%s", post_data);

    content_struct content = {
        .size = 0,
        .body = malloc(sizeof(char) * BUFFER_BLOCK_SIZE),
    };

    esp_http_client_config_t config = {
        .url = url,
        // .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = http_twitter_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = &content,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // POST
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
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

    esp_http_client_cleanup(client);
    free(content.body);
    free(auth_header);
    cJSON_Delete(root);
}
