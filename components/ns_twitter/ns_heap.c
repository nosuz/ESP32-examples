#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#define APP_HEAP_SIZE (8 * 1024)

static const char *TAG = "heap";

char heap_buffer[APP_HEAP_SIZE] = {};
char *this_pt;
char *next_pt;
static unsigned int this_size = 0;

void init_fake_heap()
{
    ESP_LOGI(TAG, "clear app heap buffer");

    this_pt = heap_buffer;
    next_pt = heap_buffer;
    this_size = 0;
}

char *fake_malloc(uint16_t size)
{
    char *tmp_pt;
    if (this_pt == NULL)
    {
        init_fake_heap();
    }

    if ((size + next_pt - heap_buffer) <= APP_HEAP_SIZE)
    {
        tmp_pt = next_pt;
        this_pt = next_pt;
        next_pt += size;
        ESP_LOGI(TAG, "malloc %d, total %d at %p, next_pt -> %p", size, next_pt - heap_buffer, this_pt, next_pt);
    }
    else
    {
        ESP_LOGE(TAG, "failed to malloc %d, max %d", size, APP_HEAP_SIZE);
        tmp_pt = NULL;
    }

    return tmp_pt;
}

void fake_free(char *pt)
{
    if (pt == this_pt)
    {
        next_pt = this_pt;
        this_size = 0;
        ESP_LOGI(TAG, "free at %p", next_pt);
    }
    else
    {
        ESP_LOGI(TAG, "failed to free");
    }
}

char *fake_realloc(char *pt, uint16_t size)
{
    char *tmp_pt;
    if (pt == this_pt)
    {
        next_pt = this_pt + size;
        this_size = size;
        tmp_pt = this_pt;
        ESP_LOGI(TAG, "realloc (expaned) %d, total %d at %p, next_pt -> %p", size, next_pt - heap_buffer, this_pt, next_pt);
    }
    else
    {
        ESP_LOGI(TAG, "realloc (copied) %d", size);
        tmp_pt = fake_malloc(size);
        if (tmp_pt != NULL)
        {
            strcpy(tmp_pt, pt);
        }
        else
        {
            ESP_LOGE(TAG, "failed to realloc");
        }
    }

    return tmp_pt;
}
