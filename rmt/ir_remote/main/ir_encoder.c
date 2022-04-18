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

        if (ir_pulses[0].on < ir_pulses[0].off)
        {
            ESP_LOGI(TAG, "Ignore noise");
        }
        else if (pulses < 2)
        {
            ESP_LOGI(TAG, "Ignore repeat command.");
        }
        else
        {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "unit", unit);
            cJSON_AddNumberToObject(root, "pulses", pulses - 1);

            cJSON *item = cJSON_CreateObject();
            cJSON_AddItemToObject(item, "on", cJSON_CreateNumber(ir_pulses[0].on));
            cJSON_AddItemToObject(item, "off", cJSON_CreateNumber(ir_pulses[0].off));
            cJSON_AddItemToObject(root, "leader", item);

            cJSON *array = cJSON_CreateArray();

            uint8_t data = 0;
            int pointer = 1;
            int bit = 0;
            int mark = 1;

            while (pointer < pulses)
            {
                if (ir_pulses[pointer].off > mark)
                    mark = ir_pulses[pointer].off;

                if (ir_pulses[pointer++].off > 1)
                    data |= (1 << bit);

                bit = (bit + 1) % 8;
                if (bit == 0)
                {
                    ESP_LOGI(TAG, "%02x", data);
                    cJSON_AddItemToArray(array, cJSON_CreateNumber(data));
                    data = 0;
                }
            }
            if (bit != 0)
            {
                ESP_LOGI(TAG, "%02x", data);
                cJSON_AddItemToArray(array, cJSON_CreateNumber(data));
            }
            cJSON_AddItemToObject(root, "data", array);
            cJSON_AddNumberToObject(root, "mark", mark);

            json_data = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
        }

        free(ir_pulses);
    }

    return json_data;
}
