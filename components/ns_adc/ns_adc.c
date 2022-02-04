#include "driver/adc_common.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

#include "ns_adc.h"

//ADC Calibration
#if CONFIG_IDF_TARGET_ESP32
#define ADC_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_VREF
#elif CONFIG_IDF_TARGET_ESP32S2
#define ADC_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32C3
#define ADC_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32S3
#define ADC_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_TP_FIT
#endif

static const char *TAG = "adc";

static esp_adc_cal_characteristics_t adc_chars;
static bool adc_cali_enable = false;

void init_adc(void)
{
    esp_err_t ret;

    ret = esp_adc_cal_check_efuse(ADC_CALI_SCHEME);
    if (ret == ESP_OK)
    {
        adc_cali_enable = true;
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc_chars);
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "Calibration scheme not supported, skip software calibration");
    }
    else if (ret == ESP_ERR_INVALID_VERSION)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg");
    }

    adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
    adc1_config_channel_atten(CONFIG_ADC1_CHANNEL, ADC_ATTEN_DB_11);
}

uint32_t read_adc_raw(void)
{
    return adc1_get_raw(CONFIG_ADC1_CHANNEL);
}

esp_err_t read_adc_voltage(uint32_t *voltage)
{
    if (!adc_cali_enable)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    *voltage = esp_adc_cal_raw_to_voltage(read_adc_raw(), &adc_chars);

    return ESP_OK;
}

uint32_t calc_adc_voltage(uint32_t raw)
{
    uint32_t voltage = 0;

    if (adc_cali_enable)
        voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

    return voltage;
}

bool calc_adc_voltage_available(void)
{
    return adc_cali_enable;
}
