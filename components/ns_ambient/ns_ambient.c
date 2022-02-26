#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"

#include "ns_http.h"
#include "ns_ambient.h"

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

void ambient_init(void)
{
    for (int i = 0; i < MAX_DATA; i++)
    {
        data[i].valid = false;
    }
    initilized_data = true;
}

esp_err_t ambient_set(unsigned int d, float value)
{
    // d: 1 to 8
    if (d > MAX_DATA)
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
    char index[] = "d?";
    // cJSON *array = cJSON_CreateArray();
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

    // POST
    ns_http_post(url, 30, "application/json", json_data, NULL);
    free(json_data);
    cJSON_Delete(root);
    free(url);

    return ESP_OK;
}
