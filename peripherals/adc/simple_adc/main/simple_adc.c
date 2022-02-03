#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ns_adc.h"

static const char *TAG = "main";

void app_main(void)
{
    int adc_raw;
    int sum_size = 16;

    uint32_t voltage;
    float battery_voltage;
    init_adc();

    ESP_LOGI(TAG, "Read ADC");
    while (1)
    {
        adc_raw = read_adc_raw();
        if (read_adc_voltage(&voltage) == ESP_OK)
        {
            printf("cali data: %2d mV ", voltage);
        }
        else
        {
            printf("raw data: %d ", adc_raw);
        }
        vTaskDelay(pdMS_TO_TICKS(100));

        adc_raw = 0;
        for (int i = 0; i < sum_size; i++)
        {
            adc_raw = adc_raw + read_adc_raw();
        }
        adc_raw = adc_raw / sum_size;

        if (calc_adc_voltage_available())
        {
            voltage = calc_adc_voltage(adc_raw);
            printf("cali avr. data: %3d mV\n", voltage);
            battery_voltage = (float)voltage * 3 / 1000;
            printf("Battery: %0.2f V\n", battery_voltage);
        }
        else
        {
            printf("raw avr. data: %d\n", adc_raw);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
