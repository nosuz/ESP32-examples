#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#define READ_ADDR(addr) ((1 << 6) | (addr << 3))
#define WRITE_ADDR(addr) (addr << 3)

static const char *TAG = "adt7310";

spi_device_handle_t dev_handle;

void adt7310_reset(void)
{
    uint8_t pattern[] = {0xff, 0xff, 0xff};
    spi_transaction_t t = {
        .cmd = 0xff,
        .length = 24, // data length
        .tx_buffer = pattern,
        .rx_buffer = NULL,
    };
    ESP_LOGD(TAG, "Reset ADT7310");
    esp_err_t err = spi_device_polling_transmit(dev_handle, &t);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to reset bus");
    }

    // wait for over 500 us.
    // System tick length will change. If the tick resolution is less than 1 ms., wait 2 ms.
    // Or wait 2 ticks. 1 tick might wait until the next tick, or less than 1 tick duration.
    vTaskDelay(pdMS_TO_TICKS(2) + 2);
}

void attach_adt7310(void)
{
    spi_device_interface_config_t dev_cfg = {
        .command_bits = 8,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 3,
        .clock_speed_hz = 500 * 1000,
        .queue_size = 1,
        .spics_io_num = CONFIG_SPI_CS,
        .pre_cb = NULL,
        .post_cb = NULL,
    };
    ESP_LOGI(TAG, "Attatch device");
    esp_err_t err = spi_bus_add_device(SPI3_HOST, &dev_cfg, &dev_handle);
    if (err == ESP_OK)
    {
        adt7310_reset();
    }
    else
    {
        ESP_LOGE(TAG, "Failed to attach device");
    }
}

uint8_t adt7310_read_status(void)
{
    uint8_t value;
    spi_transaction_t t = {
        .cmd = READ_ADDR(0x0),
        .length = 8, // data length
        .tx_buffer = NULL,
        .rx_buffer = &value,
    };
    ESP_LOGD(TAG, "Read Status from ADT7310");
    esp_err_t err = spi_device_polling_transmit(dev_handle, &t);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read status");
    }

    return value;
}

uint8_t adt7310_read_config(void)
{
    uint8_t value;
    spi_transaction_t t = {
        .cmd = READ_ADDR(0x1),
        .length = 8, // data length
        .tx_buffer = NULL,
        .rx_buffer = &value,
    };
    ESP_LOGD(TAG, "Read Config from ADT7310");
    esp_err_t err = spi_device_polling_transmit(dev_handle, &t);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read config");
    }

    return value;
}

void adt7310_write_config(uint8_t command)
{
    spi_transaction_t t = {
        .cmd = WRITE_ADDR(0x1),
        .length = 8, // data length
        .tx_buffer = &command,
        .rx_buffer = NULL,
    };
    ESP_LOGD(TAG, "Write Config from ADT7310");
    esp_err_t err = spi_device_polling_transmit(dev_handle, &t);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write config");
    }
}

uint8_t adt7310_read_id(void)
{
    uint8_t id;
    spi_transaction_t t = {
        .cmd = READ_ADDR(0x3),
        .length = 8, // data length
        .tx_buffer = NULL,
        .rx_buffer = &id,
    };
    ESP_LOGD(TAG, "Read from ADT7310");
    esp_err_t err = spi_device_polling_transmit(dev_handle, &t);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read id");
    }

    return id;
}

float adt7310_read_temp(void)
{
    // for 13-bit resolution (device default)
    float temp;
    uint8_t data[2];
    uint16_t value;
    int max_retry = 10;

    // set to cont. mode to wakeup from shutdown mode.
    adt7310_write_config(0x00);
    // set to one-shot mode.
    adt7310_write_config(0x20);
    vTaskDelay(pdMS_TO_TICKS(240));
    while (adt7310_read_status() & 0x80)
    {
        max_retry--;
        if (max_retry == 0)
        {
            ESP_LOGE(TAG, "Give up waiting convertion");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    spi_transaction_t t = {
        .cmd = READ_ADDR(0x2),
        .length = 16, // data length
        .tx_buffer = NULL,
        .rx_buffer = data,
    };
    ESP_LOGD(TAG, "Read Temp from ADT7310");
    esp_err_t err = spi_device_polling_transmit(dev_handle, &t);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read temp");
    }

    // printf("raw: %02x %02x\n", data[0], data[1]);
    value = (data[0] << 8) + data[1];
    // printf("value: %04x\n", value);
    // >>3 removes event flags
    value = value >> 3;
    // printf("tmp: %04x\n", value);
    if (value & 0x1000)
    {
        temp = (float)value - 8192;
    }
    else
    {
        temp = (float)value;
    }
    temp /= 16;

    return temp;
}
