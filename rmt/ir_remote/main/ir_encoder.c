#include <stdio.h>
#include <math.h>

#include "esp_log.h"
#include "driver/rmt.h"
#include "cJSON.h"

static const char *TAG = "encoder";

typedef struct
{
    int on;
    int off;
} IR_PULSE;

static void pack_ir_data(cJSON *root, IR_PULSE *ir_pulses, size_t pulses);
static void raw_ir_data(cJSON *root, IR_PULSE *ir_pulses, size_t pulses);

char *encode_ir_pulses(rmt_item32_t *items, size_t length)
{
    int unit = 0;
    int pulses = 0;
    if (items[0].duration1 != 0)
    {
        for (pulses = 1; pulses < length; pulses++)
        {
            unit += items[pulses].duration0;

            if (items[pulses].duration1 == 0)
            {
                break;
            }
        }
        unit /= pulses;
    }

    char *json_data = NULL;
    if (pulses > 0)
    {
        cJSON *root = cJSON_CreateObject();

        ESP_LOGI(TAG, "-----------------------------------");
        ESP_LOGI(TAG, "Unit: %d, Data pulses: %d", unit, pulses);

        IR_PULSE *ir_pulses;
        ir_pulses = malloc(sizeof(IR_PULSE) * pulses);
        for (int i = 0; i < pulses; i++)
        {
            ir_pulses[i].on = (int)round((float)items[i].duration0 / unit);
            ir_pulses[i].off = (int)round((float)items[i].duration1 / unit);

            ESP_LOGI(TAG, "ON: %d(%d), OFF: %d(%d)",
                     items[i].duration0, ir_pulses[i].on,
                     items[i].duration1, ir_pulses[i].off);
        }

        if ((ir_pulses[0].on >= 13) && (ir_pulses[0].off >= 6))
        {
            ESP_LOGI(TAG, "NEC format");
            // 950nm, 38kHz, duty 1:2, leader ON 16 and OFF 8
            cJSON_AddStringToObject(root, "formt", "NEC");
            cJSON_AddFalseToObject(root, "repeat");
            pack_ir_data(root, ir_pulses, pulses);
        }
        else if ((ir_pulses[0].on >= 13) && (ir_pulses[0].off >= 3) && (pulses == 1))
        {
            ESP_LOGI(TAG, "NEC format (Repeated)");
            // 950nm, 38kHz, duty 1:2, ON 16 and OFF 4 followed by 1 ON
            cJSON_AddStringToObject(root, "formt", "NEC");
            cJSON_AddTrueToObject(root, "repeat");
        }
        else if ((ir_pulses[0].on >= 6) && (ir_pulses[0].off >= 2))
        {
            ESP_LOGI(TAG, "AEHA format");
            // 950nm, 38kHz, duty 1:2, leader ON 8 and  OFF 4
            cJSON_AddStringToObject(root, "formt", "AEHA");
            cJSON_AddFalseToObject(root, "repeat");
            pack_ir_data(root, ir_pulses, pulses);
        }
        else if ((ir_pulses[0].on >= 6) && (ir_pulses[0].off >= 6) && (pulses == 1))
        {
            ESP_LOGI(TAG, "AEHA format (Repeated");
            // 950nm, 38kHz, duty 1:2, ON 8 and OFF 8 followed by 1 ON
            cJSON_AddStringToObject(root, "formt", "AEHA");
            cJSON_AddTrueToObject(root, "repeat");
        }
        else
        {
            ESP_LOGI(TAG, "Unkown format");
            cJSON_AddStringToObject(root, "formt", "raw");
            cJSON_AddFalseToObject(root, "repeat");
            raw_ir_data(root, ir_pulses, pulses);
        }
        cJSON_AddNumberToObject(root, "unit", unit);

        free(ir_pulses);
        json_data = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
    }

    return json_data;
}

static void pack_ir_data(cJSON *root, IR_PULSE *ir_pulses, size_t pulses)
{
    uint8_t data = 0;
    int pointer = 1;
    int bit = 0;

    cJSON *array = cJSON_CreateArray();
    while (1)
    {
        if (ir_pulses[pointer++].off > 1)
            data |= (1 << bit);

        if (pointer >= pulses)
        {
            ESP_LOGI(TAG, "%02x", data);
            cJSON_AddItemToArray(array, cJSON_CreateNumber(data));
            break;
        }
        else
        {
            bit = (bit + 1) % 8;
            if (bit == 0)
            {
                ESP_LOGI(TAG, "%02x", data);
                cJSON_AddItemToArray(array, cJSON_CreateNumber(data));
                data = 0;
            }
        }
    }
    cJSON_AddItemToObject(root, "data", array);
}

static void raw_ir_data(cJSON *root, IR_PULSE *ir_pulses, size_t pulses)
{
    cJSON *array = cJSON_CreateArray();
    for (int i = 0; i < pulses; i++)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddItemToObject(item, "on", cJSON_CreateNumber(ir_pulses[i].on));
        cJSON_AddItemToObject(item, "off", cJSON_CreateNumber(ir_pulses[i].off));
        cJSON_AddItemToArray(array, item);
    }
    cJSON_AddItemToObject(root, "pulses", array);
}
