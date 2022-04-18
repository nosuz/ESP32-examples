#include <stdio.h>

#include "esp_log.h"
#include "driver/rmt.h"
#include "cJSON.h"

static const char *TAG = "decoder";

void decode_ir_pulses(const char *params, rmt_item32_t **items, size_t *length)
{
    int unit = 300;
    int pulses = 0;
    int mark = 2;

    if (*items != NULL)
    {
        free(*items);
        *items = NULL;
    }
    *length = 0;

    if (params == NULL)
        return;

    cJSON *root = cJSON_Parse(params);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "JSON parse error");
        return;
    }

    cJSON *unit_json = cJSON_GetObjectItemCaseSensitive(root, "unit");
    if (cJSON_IsNumber(unit_json))
        unit = unit_json->valueint;

    cJSON *mark_json = cJSON_GetObjectItemCaseSensitive(root, "mark");
    if (cJSON_IsNumber(mark_json))
        mark = mark_json->valueint;

    cJSON *pulses_json = cJSON_GetObjectItemCaseSensitive(root, "pulses");
    if (cJSON_IsNumber(pulses_json))
        pulses = pulses_json->valueint;

    *length = pulses + 2; // append space for leader and trailer
    *items = malloc(sizeof(rmt_item32_t) * (*length));

    cJSON *leader_json = cJSON_GetObjectItemCaseSensitive(root, "leader");

    (*items)[0].level0 = 1;
    cJSON *leader_on_json = cJSON_GetObjectItemCaseSensitive(leader_json, "on");
    if (cJSON_IsNumber(leader_on_json))
        (*items)[0].duration0 = unit * leader_on_json->valueint;

    (*items)[0].level1 = 0;
    cJSON *leader_off_json = cJSON_GetObjectItemCaseSensitive(leader_json, "off");
    if (cJSON_IsNumber(leader_off_json))
        (*items)[0].duration1 = unit * leader_off_json->valueint;

    cJSON *array = cJSON_GetObjectItem(root, "data");
    int index, bit, value;
    for (int i = 0; i < pulses; i++)
    {
        (*items)[i + 1].level0 = 1;
        (*items)[i + 1].duration0 = unit;
        (*items)[i + 1].level1 = 0;

        index = i / 8;
        bit = i % 8;
        value = cJSON_GetArrayItem(array, index)->valueint;
        (*items)[i + 1].duration1 = unit * (value & (1 << bit) ? mark : 1);
    }

    cJSON_Delete(root);

    // Add trailer
    (*items)[pulses + 1].level0 = 1;
    (*items)[pulses + 1].duration0 = unit;
    (*items)[pulses + 1].level1 = 0;
    (*items)[pulses + 1].duration1 = 0;

    // for (int i = 0; i < *length; i++)
    // {
    //     ESP_LOGI(TAG, "ON: %d, OFF: %d",
    //              (*items)[i].duration0,
    //              (*items)[i].duration1);
    // }
}