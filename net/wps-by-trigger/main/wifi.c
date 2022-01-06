/* This code is from ESP-IDF example.

   This code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
// #include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"

#include "esp_wps.h"

#include "gpio.h"

#define WPS_MODE WPS_TYPE_PBC

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define ESP_MAXIMUM_RETRY CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi";

static esp_wps_config_t wps_config = WPS_CONFIG_INIT_DEFAULT(WPS_MODE);
static wifi_config_t wps_ap_creds[MAX_WPS_AP_CRED];
static int s_ap_creds_num = 0;
static int s_retry_num = 0;
static bool wifi_stop_flag = false;
static bool initialized_wifi = false;
static bool started_wifi = false;
static bool wps_in_progress = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    static int ap_idx = 1;

    /* events are defined in
    esp-idf/components/esp_wifi/include/esp_wifi_types.h
    */
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START: connect to the AP");
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_STOP:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_STOP: stoped wifi");
        wifi_stop_flag = false;
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED: connected to the AP");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED: disconnected from the AP");
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (wifi_stop_flag)
        {
            // skip retry connect if esp_wifi_stop is issued.
            ESP_LOGI(TAG, "Stop wifi in progress. Give up reconnection.");
        }
        if (wps_in_progress)
        {
            // skip retry connect while WPS in progress
            ESP_LOGI(TAG, "WPS in progress. Give up reconnection.");
        }
        else if (s_retry_num < ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP: %d", s_retry_num);
        }
        else if (ap_idx < s_ap_creds_num)
        {
            /* Try the next AP credential if first one fails */

            if (ap_idx < s_ap_creds_num)
            {
                ESP_LOGI(TAG, "Connecting to SSID: %s", wps_ap_creds[ap_idx].sta.ssid);
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wps_ap_creds[ap_idx++]));
                esp_wifi_connect();
            }
            s_retry_num = 0;
        }
        else
        {
            ESP_LOGI(TAG, "failed to connect the AP");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        break;
    case WIFI_EVENT_STA_WPS_ER_SUCCESS:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_WPS_ER_SUCCESS");
        wifi_event_sta_wps_er_success_t *evt =
            (wifi_event_sta_wps_er_success_t *)event_data;
        int i;

        if (evt)
        {
            s_ap_creds_num = evt->ap_cred_cnt;
            for (i = 0; i < s_ap_creds_num; i++)
            {
                memcpy(wps_ap_creds[i].sta.ssid, evt->ap_cred[i].ssid,
                       sizeof(evt->ap_cred[i].ssid));
                memcpy(wps_ap_creds[i].sta.password, evt->ap_cred[i].passphrase,
                       sizeof(evt->ap_cred[i].passphrase));
            }
            /* If multiple AP credentials are received from WPS, connect with first one */
            ESP_LOGI(TAG, "Connecting to SSID: %s, Passphrase: %s",
                     wps_ap_creds[0].sta.ssid, wps_ap_creds[0].sta.password);
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wps_ap_creds[0]));
        }
        /*
        * If only one AP credential is received from WPS, there will be no event data and
        * esp_wifi_set_config() is already called by WPS modules for backward compatibility
        * with legacy apps. So directly attempt connection here.
        */
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        wps_in_progress = false;
        stop_blink();

        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_WPS_ER_FAILED:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_WPS_ER_FAILED");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&wps_config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_WPS_ER_TIMEOUT");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&wps_config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;
    // case WIFI_EVENT_STA_WPS_ER_PIN:
    //     ESP_LOGI(TAG, "WIFI_EVENT_STA_WPS_ER_PIN");
    //     /* display the PIN code */
    //     wifi_event_sta_wps_er_pin_t *event = (wifi_event_sta_wps_er_pin_t *)event_data;
    //     ESP_LOGI(TAG, "WPS_PIN = " PINSTR, PIN2STR(event->pin_code));
    //     break;
    case WIFI_EVENT_STA_WPS_ER_PIN:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_WPS_ER_PIN");
        break;
    case WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP");
        break;
    default:
        ESP_LOGI(TAG, "event_base: WIFI_EVENT, id %d", event_id);
        break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
}

int wifi_init(void)
{
    int nvs_error = 0;
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // enable Wi-Fi NVS flash in menuconfig, Componet config -> Wi-Fi
    // enabled in default.
    wifi_config_t wifi_config;
    esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);
    if (strlen(&wifi_config.sta.ssid) == 0 || strlen(&wifi_config.sta.password) == 0)
    {
        ESP_LOGI(TAG, "get into WPS mode.");
        // wifi_config = WPS_CONFIG_INIT_DEFAULT(WPS_MODE);
        nvs_error = 1;
    }
    else
    {
        ESP_LOGI(TAG, "use ssid and password stored in nvs. SSID: %s", wifi_config.sta.ssid);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    }

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));

    initialized_wifi = true;
    ESP_LOGI(TAG, "wifi_init finished.");

    return nvs_error;
}

void wifi_connect(void)
{
    if (initialized_wifi)
    {
        if (!started_wifi)
        {
            ESP_ERROR_CHECK(esp_wifi_start());
            started_wifi = true;
            ESP_LOGI(TAG, "wifi started.");
        }
    }
    else
    {
        ESP_LOGE(TAG, "wifi not initialized.");
    }
}

void wifi_wps_start(void)
{
    if (!started_wifi)
    {
        wifi_connect();
    }
    wps_in_progress = true;
    esp_wifi_disconnect();

    ESP_LOGI(TAG, "WPS start");
    start_blink();
    ESP_ERROR_CHECK(esp_wifi_wps_disable());
    ESP_ERROR_CHECK(esp_wifi_wps_enable(&wps_config));
    ESP_ERROR_CHECK(esp_wifi_wps_start(0));
}

int wifi_wait_connection(void)
{
    int connected = 0;
    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        connected = 1;
        ESP_LOGI(TAG, "Connected to AP");
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect AP");
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    return connected;
}

void wifi_disconnect(void)
{
    if (wps_in_progress)
    {
        while (wps_in_progress)
        {
            ESP_LOGI(TAG, "waiting WPS.");
            vTaskDelay(2 * 1000 / portTICK_PERIOD_MS);
        }
        vTaskDelay(3 * 1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "wifi stop.");
    wifi_stop_flag = true;
    started_wifi = false;
    // ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler));
    esp_wifi_stop();
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
}
