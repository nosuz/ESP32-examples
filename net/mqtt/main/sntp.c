#include <string.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/semphr.h"

#define SNTP_SERVER CONFIG_SNTP_SERVER
// #define SNTP_INTERVAL (CONFIG_SNTP_INTERVAL * 1000)

static const char *TAG = "sntp";

static SemaphoreHandle_t sntp_semaphore = NULL;

void time_synced_callback(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time Synced");
    xSemaphoreGive(sntp_semaphore);
}

void init_sntp(void)
{
    /**
     * NTP server address could be aquired via DHCP,
     * see LWIP_DHCP_GET_NTP_SRV menuconfig option
     */
#ifdef LWIP_DHCP_GET_NTP_SRV
    sntp_servermode_dhcp(1);
#endif

    ESP_LOGI(TAG, "Init SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, SNTP_SERVER);
    sntp_set_time_sync_notification_cb(time_synced_callback);
    // sntp_set_sync_interval(SNTP_INTERVAL); // default once per hour.

    sntp_semaphore = xSemaphoreCreateBinary();
}

void stop_sntp(void)
{
    ESP_LOGI(TAG, "Stop SNTP");
    sntp_stop();
}

void start_sntp(void)
{
    ESP_LOGI(TAG, "Start SNTP");
    sntp_init();

    // wait for time to be set
    ESP_LOGI(TAG, "Waiting for system time to be set");
    if (xSemaphoreTake(sntp_semaphore, pdMS_TO_TICKS(20 * 1000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to set systemtime");
    }

    stop_sntp();
}
