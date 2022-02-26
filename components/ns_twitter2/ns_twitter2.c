#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "ns_http.h"
#include "ns_sntp.h"
#include "ns_twitter2.h"

#define MAX_PARAMS 16

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

    HTTP_CONTENT content = {
        .size = 0,
        .body = NULL,
    };

#ifdef CONFIG_TWITTER_PRIVATE_CLIENT
    esp_err_t err = ns_http_auth_basic_post(url, client_id, client_secret, 0, "application/x-www-form-urlencoded", post_data, &content);
#else
    esp_err_t err = ns_http_post(url, 0, "application/x-www-form-urlencoded", post_data, &content);
#endif
    if (err == ESP_OK)
    {
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

    cJSON *root = cJSON_CreateObject();
    for (int i = 0; i < param_index; i++)
    {
        cJSON_AddStringToObject(root, api_params[i].key, api_params[i].value);
    }
    const char *post_data = cJSON_PrintUnformatted(root);
    // printf("%s", post_data);

    // POST
    ns_http_auth_bearer_post(url, access_token, 0, "application/json", post_data, NULL);

    cJSON_Delete(root);
}
