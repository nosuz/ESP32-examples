#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc_common.h"
#include "esp_adc_cal.h"

#define ADC1_CHANNEL CONFIG_ADC1_CHANNEL

//ADC Calibration
#if CONFIG_IDF_TARGET_ESP32
#define ADC_EXAMPLE_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_VREF
#elif CONFIG_IDF_TARGET_ESP32S2
#define ADC_EXAMPLE_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32C3
#define ADC_EXAMPLE_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32S3
#define ADC_EXAMPLE_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_TP_FIT
#endif

static const char *TAG = "adc";

static esp_adc_cal_characteristics_t adc_chars;

static bool adc_init(void)
{
    esp_err_t ret;
    bool cali_enable = false;

    ret = esp_adc_cal_check_efuse(ADC_EXAMPLE_CALI_SCHEME);
    if (ret == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "Calibration scheme not supported, skip software calibration");
    }
    else if (ret == ESP_ERR_INVALID_VERSION)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else if (ret == ESP_OK)
    {
        cali_enable = true;
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc_chars);
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg");
    }

    return cali_enable;
}

void app_main(void)
{
    int adc_raw;
    int sum_size = 16;

    uint32_t voltage;
    bool cali_enable = adc_init();

    //ADC1 config
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL, ADC_ATTEN_DB_11));

    ESP_LOGI(TAG, "Read ADC");
    while (1)
    {
        adc_raw = adc1_get_raw(ADC1_CHANNEL);
        if (cali_enable)
        {
            voltage = esp_adc_cal_raw_to_voltage(adc_raw, &adc_chars);
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
            adc_raw = adc_raw + adc1_get_raw(ADC1_CHANNEL);
        }
        adc_raw = adc_raw / sum_size;
        if (cali_enable)
        {
            voltage = esp_adc_cal_raw_to_voltage(adc_raw, &adc_chars);
            printf("cali avr. data: %3d mV\n", voltage);
        }
        else
        {
            printf("raw avr. data: %d\n", adc_raw);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
