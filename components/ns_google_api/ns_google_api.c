#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"

#include "ns_http.h"
#include "ns_sntp.h"

#include "ns_google_api.h"

#define MAX_PARAMS 16
#define MAX_URL_LENGTH 250

#define GOOGLE_CLIENT_ID CONFIG_GOOGLE_CLIENT_ID
#define GOOGLE_CLIENT_SECRET CONFIG_GOOGLE_CLIENT_SECRET
#define GOOGLE_REFRESH_TOKEN CONFIG_GOOGLE_REFHRESH_TOKEN

#define KEY_CLIENT_ID "client_id"
#define KEY_CLIENT_SECRET "client_sec"
#define KEY_INIT_REFRESH_TOKEN "init_refresh"
#define KEY_REFRESH_TOKEN "refresh_token"
#define KEY_ACCESS_TOKEN "access_token"
#define KEY_LAST_REFRESHED "refreshed_at"

// #define DEBUG

static const char *TAG = "google_api";

static char *client_id;
static char *client_secret;
static char *refresh_token;
static char *access_token;
nvs_handle_t google_nvs_handle;

typedef struct
{
    char *key;
    char *value;
} GOOGLE_API_PARAM;

static int param_index = 0;
GOOGLE_API_PARAM api_params[MAX_PARAMS];

esp_err_t google_api_refresh_token(void)
{
    const char *url = "https://oauth2.googleapis.com/token";

    char *post_data;
    post_data = malloc(strlen("grant_type=refresh_token") + strlen("refresh_token=") + strlen(refresh_token) + strlen("client_id=") + strlen(client_id) + strlen("client_secret=") + strlen(client_secret) + 4);
    strcpy(post_data, "grant_type=refresh_token&refresh_token=");
    strcat(post_data, refresh_token);
    strcat(post_data, "&client_id=");
    strcat(post_data, client_id);
    strcat(post_data, "&client_secret=");
    strcat(post_data, client_secret);

    HTTP_STRUCT *http = ns_http_post(url, "application/x-www-form-urlencoded", post_data, strlen(post_data));

    HTTP_CONTENT content = ns_http_default_content();

    // ESP_LOGI(TAG, "free heap(Before Refresh Token): %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    // ESP_LOGI(TAG, "stack level(Before Refresh Token): %d", uxTaskGetStackHighWaterMark(NULL));
    esp_err_t err = ns_http_send(http, &content);
    // ESP_LOGI(TAG, "free heap(After Refresh Token): %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    // ESP_LOGI(TAG, "stack level(After Refresh Token): %d", uxTaskGetStackHighWaterMark(NULL));
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
                    ESP_LOGI(TAG, "Refreshed Access Token: %s", access_token);
                }

                new_refresh_token = cJSON_GetObjectItemCaseSensitive(res_json, "refresh_token");
                if (cJSON_IsString(new_refresh_token) && (new_refresh_token->valuestring != NULL))
                {
                    if (refresh_token != NULL)
                        free(refresh_token);
                    refresh_token = malloc(strlen(new_refresh_token->valuestring) + 1);
                    strcpy(refresh_token, new_refresh_token->valuestring);
                    ESP_LOGI(TAG, "Refreshed Refresh Token: %s", refresh_token);
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
    ns_http_content_cleanup(&content);

    return err;
}

esp_err_t google_api_init(void)
{
    init_sntp(3600);
    // update system clock
    start_sntp();

    esp_err_t err = nvs_open("google", NVS_READWRITE, &google_nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Opening NVS error");
        return ESP_FAIL;
    }

    bool new_params = false;
    size_t required_size;
    uint32_t last_update;
    err = nvs_get_u32(google_nvs_handle, KEY_LAST_REFRESHED, &last_update);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "No params in NVS");
        new_params = true;
    }
    else if (client_id == NULL)
    {
        ESP_LOGI(TAG, "Validate Google params");

        nvs_get_str(google_nvs_handle, KEY_CLIENT_ID, NULL, &required_size);
        client_id = malloc(required_size);
        nvs_get_str(google_nvs_handle, KEY_CLIENT_ID, client_id, &required_size);
        if (strcmp(client_id, GOOGLE_CLIENT_ID) != 0)
            new_params = true;

        nvs_get_str(google_nvs_handle, KEY_CLIENT_SECRET, NULL, &required_size);
        client_secret = malloc(required_size);
        nvs_get_str(google_nvs_handle, KEY_CLIENT_SECRET, client_secret, &required_size);
        if (strcmp(client_secret, GOOGLE_CLIENT_SECRET) != 0)
            new_params = true;

        nvs_get_str(google_nvs_handle, KEY_INIT_REFRESH_TOKEN, NULL, &required_size);
        char *init_refresh_token;
        init_refresh_token = malloc(required_size);
        nvs_get_str(google_nvs_handle, KEY_INIT_REFRESH_TOKEN, init_refresh_token, &required_size);
        if (strcmp(init_refresh_token, GOOGLE_REFRESH_TOKEN) != 0)
            new_params = true;
        free(init_refresh_token);
    }

    if (new_params)
    {
        // update NVS
        ESP_LOGI(TAG, "Update Google API params in NVS");

        if (client_id != NULL)
            free(client_id);
        client_id = malloc(strlen(GOOGLE_CLIENT_ID) + 1);
        strcpy(client_id, GOOGLE_CLIENT_ID);
        nvs_set_str(google_nvs_handle, KEY_CLIENT_ID, client_id);

        if (client_secret != NULL)
            free(client_secret);
        client_secret = malloc(strlen(GOOGLE_CLIENT_SECRET) + 1);
        strcpy(client_secret, GOOGLE_CLIENT_SECRET);
        nvs_set_str(google_nvs_handle, KEY_CLIENT_SECRET, client_secret);

        nvs_set_str(google_nvs_handle, KEY_INIT_REFRESH_TOKEN, GOOGLE_REFRESH_TOKEN);

        refresh_token = malloc(strlen(GOOGLE_REFRESH_TOKEN) + 1);
        strcpy(refresh_token, GOOGLE_REFRESH_TOKEN);
    }
    else
    {
        // get from NVS
        ESP_LOGI(TAG, "Use Google API params in NVS");

        if (client_id == NULL)
        {
            nvs_get_str(google_nvs_handle, KEY_CLIENT_ID, NULL, &required_size);
            client_id = malloc(required_size);
            nvs_get_str(google_nvs_handle, KEY_CLIENT_ID, client_id, &required_size);

            nvs_get_str(google_nvs_handle, KEY_CLIENT_SECRET, NULL, &required_size);
            client_secret = malloc(required_size);
            nvs_get_str(google_nvs_handle, KEY_CLIENT_SECRET, client_secret, &required_size);
        }

        nvs_get_str(google_nvs_handle, KEY_REFRESH_TOKEN, NULL, &required_size);
        refresh_token = malloc(required_size);
        nvs_get_str(google_nvs_handle, KEY_REFRESH_TOKEN, refresh_token, &required_size);

        nvs_get_str(google_nvs_handle, KEY_ACCESS_TOKEN, NULL, &required_size);
        access_token = malloc(required_size);
        nvs_get_str(google_nvs_handle, KEY_ACCESS_TOKEN, access_token, &required_size);
    }

    time_t now;
    time(&now);
    if (new_params || (now > (last_update + CONFIG_GOOGLE_TOKEN_EXPIRE_TIME - 180)))
    {
        ESP_LOGI(TAG, "Refresh OAuth token");
        // expired, refresh token.
        err = google_api_refresh_token();
        if (err == ESP_OK)
        {
            new_params = true;

            nvs_set_u32(google_nvs_handle, KEY_LAST_REFRESHED, now);
            nvs_set_str(google_nvs_handle, KEY_ACCESS_TOKEN, access_token);
            nvs_set_str(google_nvs_handle, KEY_REFRESH_TOKEN, refresh_token);
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
        err = nvs_commit(google_nvs_handle);
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
    nvs_close(google_nvs_handle);
    return err;
}

void google_api_params_init(void)
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

void google_api_param(const char *key, const char *value)
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

// google_shreadsheet_append(SPREADSHEET_ID,  SHEET_RANGE, VALUES);
void google_shreadsheet_append(const char *spreadsheet_id, const char *sheet_range, const char *values)
{
    const char *url_template = "https://sheets.googleapis.com/v4/spreadsheets/%s/values/%s:append?insertDataOption=INSERT_ROWS&valueInputOption=USER_ENTERED";

    char *url;
    url = malloc(MAX_URL_LENGTH);

    snprintf(url, MAX_URL_LENGTH, url_template,
             spreadsheet_id,
             sheet_range);
    ESP_LOGI(TAG, "URL: %s", url);
    ESP_LOGI(TAG, "values: %s", values);

    // POST
    HTTP_STRUCT *http = ns_http_post(url, "application/json", values, strlen(values));
    ns_http_auth_bearer(http, access_token);
    ns_http_send(http, NULL);

    free(url);
}
