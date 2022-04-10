#include <string.h>
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>

#include "esp_sntp.h"
#include "freertos/semphr.h"

#include "ns_sntp.h"

#define SNTP_INTERVAL (CONFIG_SNTP_INTERVAL * 1000)

RTC_DATA_ATTR static time_t last_synced = 0;

static const char *TAG = "sntp";

static SemaphoreHandle_t sntp_semaphore = NULL;
uint16_t interval = 3600;

void time_synced_callback(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time Synced");
    xSemaphoreGive(sntp_semaphore);
}

void set_local_timezone(void)
{
    setenv("TZ", CONFIG_TIME_ZONE, 1);
    // setenv("TZ", "JST-9", 1);
    tzset();
}

void init_sntp(uint16_t sntp_interval)
{
    ESP_LOGI(TAG, "Init SNTP");
    set_local_timezone();

#ifdef LWIP_DHCP_GET_NTP_SRV
    /**
     * NTP server address could be aquired via DHCP,
     * see LWIP_DHCP_GET_NTP_SRV menuconfig option
     */
    sntp_servermode_dhcp(1);
#endif

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, CONFIG_SNTP_SERVER);
    sntp_set_time_sync_notification_cb(time_synced_callback);
    // sntp_set_sync_interval(interval); // default once per hour.
    interval = sntp_interval;
    sntp_set_sync_interval(sntp_interval * 1000); // set as ms
}

void sync_sntp(void)
{
    time_t now;
    struct tm timeinfo;

    sntp_semaphore = xSemaphoreCreateBinary();

    // Get systemtime into `now` and set local time in `timeinfo`
    time(&now);
    localtime_r(&now, &timeinfo);
    if ((timeinfo.tm_year < (2016 - 1900)) || ((now - last_synced) > interval))
    {
        ESP_LOGW(TAG, "Update systemtime.");

        ESP_LOGI(TAG, "Start SNTP");
        sntp_init();
        // wait for time to be set
        ESP_LOGI(TAG, "Waiting for system time to be set");
        if (xSemaphoreTake(sntp_semaphore, pdMS_TO_TICKS(20 * 1000)) == pdTRUE)
        {
            time(&now);
            last_synced = now;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to set systemtime");
        }
    }
    else
    {
        ESP_LOGI(TAG, "Skip updating systemtime.");
        // xSemaphoreGive(sntp_semaphore);
    }

    stop_sntp();
}

void start_sntp(void)
{
    // Start SNPT service and update system time every specified intervals.
    sntp_semaphore = xSemaphoreCreateBinary();

    ESP_LOGI(TAG, "Start SNTP");
    sntp_init();
    // wait for time to be set
    ESP_LOGI(TAG, "Waiting for system time to be set");
    if (xSemaphoreTake(sntp_semaphore, pdMS_TO_TICKS(20 * 1000)) == pdTRUE)
    {
        ESP_LOGI(TAG, "Updated system time");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to set systemtime");
    }
}

void stop_sntp(void)
{
    ESP_LOGI(TAG, "Stop SNTP");
    sntp_stop();
}
