#include "nvs_flash.h"

#include "esp_log.h"
#include "nvs_flash.h"

static char *TAG = "nvs";

//Initialize NVS
void nvs_init(void)
{
#ifdef CONFIG_FORCE_ERASE_NVS
    ESP_LOGI(TAG, "Erase NVS.");
    ESP_ERROR_CHECK(nvs_flash_erase());
#else
    ESP_LOGI(TAG, "Keep NVS.");
#endif
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}
