#include <stdio.h>

#include "esp_log.h"
#include "driver/rmt.h"
#include "cJSON.h"

#include "ir_decoder.h"

static const char *TAG = "decoder";

RMT_ITEMS *decode_ir_pulses(const char *params)
{
    RMT_ITEMS *rmt_items = NULL;

    int unit = 300;
    int pulses = 0;
    int mark = 2;

    if (params == NULL)
        return rmt_items;

    cJSON *root = cJSON_Parse(params);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "JSON parse error");
        return rmt_items;
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

    rmt_items = malloc(sizeof(RMT_ITEMS));
    rmt_items->length = pulses + 2; // append space for leader and trailer
    rmt_items->items = malloc(sizeof(rmt_item32_t) * (rmt_items->length));

    cJSON *leader_json = cJSON_GetObjectItemCaseSensitive(root, "leader");

    rmt_items->items[0].level0 = 1;
    cJSON *leader_on_json = cJSON_GetObjectItemCaseSensitive(leader_json, "on");
    if (cJSON_IsNumber(leader_on_json))
        rmt_items->items[0].duration0 = unit * leader_on_json->valueint;

    rmt_items->items[0].level1 = 0;
    cJSON *leader_off_json = cJSON_GetObjectItemCaseSensitive(leader_json, "off");
    if (cJSON_IsNumber(leader_off_json))
        rmt_items->items[0].duration1 = unit * leader_off_json->valueint;

    cJSON *array = cJSON_GetObjectItem(root, "data");
    int index, bit, value;
    for (int i = 0; i < pulses; i++)
    {
        rmt_items->items[i + 1].level0 = 1;
        rmt_items->items[i + 1].duration0 = unit;
        rmt_items->items[i + 1].level1 = 0;

        index = i / 8;
        bit = i % 8;
        value = cJSON_GetArrayItem(array, index)->valueint;
        rmt_items->items[i + 1].duration1 = unit * (value & (1 << bit) ? mark : 1);
    }

    cJSON_Delete(root);

    // Add trailer
    rmt_items->items[pulses + 1].level0 = 1;
    rmt_items->items[pulses + 1].duration0 = unit;
    rmt_items->items[pulses + 1].level1 = 0;
    rmt_items->items[pulses + 1].duration1 = 0;

    // for (int i = 0; i < *length; i++)
    // {
    //     ESP_LOGI(TAG, "ON: %d, OFF: %d",
    //             rmt_items->items[i].duration0,
    //             rmt_items->items[i].duration1);
    // }

    return rmt_items;
}

void delete_rmt_items(RMT_ITEMS *rmt_items)
{
    if (rmt_items == NULL)
        return;

    free(rmt_items->items);
    free(rmt_items);
}