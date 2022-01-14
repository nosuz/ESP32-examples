#include "driver/spi_common.h"
#include "esp_log.h"

static const char *TAG = "spi";

void init_spi(void)
{
    spi_bus_config_t bus_cfg = {
        .miso_io_num = CONFIG_SPI_MISO,
        .mosi_io_num = CONFIG_SPI_MOSI,
        .sclk_io_num = CONFIG_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
        .intr_flags = 0,
    };
    ESP_LOGI(TAG, "Init SPI bus");
    spi_bus_initialize(SPI3_HOST, &bus_cfg, 0);
}
