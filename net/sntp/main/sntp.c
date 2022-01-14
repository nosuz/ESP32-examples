#include <string.h>
#include "esp_log.h"
#include "esp_sntp.h"

#define SNTP_SERVER CONFIG_SNTP_SERVER
#define SNTP_INTERVAL (CONFIG_SNTP_INTERVAL * 1000)

static const char *TAG = "sntp";

void time_synced_callback(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time Synced Event");
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
    sntp_set_sync_interval(SNTP_INTERVAL); // default once per hour.
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
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

#ifdef CONFIG_SNTP_ONESHOT
    stop_sntp();
#endif
}
