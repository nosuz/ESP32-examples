/* This code is from ESP-IDF example.

   This code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
// #include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "esp_sleep.h"

#include "ns_nvs.h"
#include "ns_gpio.h"
#include "ns_spiffs.h"
#include "ns_ap_select_web.h"

#include "ns_wifi.h"

#define WPS_MODE WPS_TYPE_PBC

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "wifi";

RTC_DATA_ATTR static int s_retry_num = 0;
RTC_DATA_ATTR static int sleep_length = 0;

RTC_DATA_ATTR static bool new_ap_param = false;
RTC_DATA_ATTR static bool start_ap_select = false;

wifi_config_t wifi_ap_configs[MAX_WPS_AP_CRED];
int s_ap_creds_num = 0;
bool wifi_stop_flag = false;
bool initialized_wifi = false;
bool started_wifi = false;
bool wps_in_progress = false;
bool ap_select_mode = false;
SemaphoreHandle_t restart_semaphore = NULL;

uint16_t ap_count = 0;
wifi_ap_record_t ap_info[CONFIG_SCAN_AP_LIST_SIZE];

void wifi_event_handler(void *arg, esp_event_base_t event_base,
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
        else if (wps_in_progress)
        {
            // skip retry connect while WPS in progress
            ESP_LOGI(TAG, "WPS in progress. Give up reconnection.");
        }
        else if (ap_select_mode)
        {
            // skip retry connect while WPS in progress
            ESP_LOGI(TAG, "AP select mode. Give up reconnection.");
        }
        else if (ap_idx < s_ap_creds_num)
        {
            /* Try the next AP credential if first one fails */
            ESP_LOGI(TAG, "Connecting to SSID: %s", wifi_ap_configs[ap_idx].sta.ssid);
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_ap_configs[ap_idx++]));
            esp_wifi_connect();
        }
        else
        {
            if (s_retry_num == 0)
                sleep_length = 0;

            if (new_ap_param)
            {
                ESP_LOGE(TAG, "Set wrong AP param.");
                start_ap_select = true;
                // esp_restart();
                esp_sleep_enable_timer_wakeup(1000000LL);
            }
            else
            {
                s_retry_num++;
                ESP_LOGW(TAG, "retry to connect to the AP: %d", s_retry_num);
                if (s_retry_num < CONFIG_AP_MAXIMUM_RETRY)
                {
                    sleep_length += s_retry_num * 20;
                    ESP_LOGI(TAG, "Sleep %d sec.", sleep_length);
                    esp_sleep_enable_timer_wakeup(1000000LL * sleep_length);
                }
                else
                {
                    ESP_LOGE(TAG, "failed to connect the AP");
                    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
                }
            }
            esp_deep_sleep_start();
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
                memcpy(wifi_ap_configs[i].sta.ssid, evt->ap_cred[i].ssid,
                       sizeof(evt->ap_cred[i].ssid));
                memcpy(wifi_ap_configs[i].sta.password, evt->ap_cred[i].passphrase,
                       sizeof(evt->ap_cred[i].passphrase));
            }
            /* If multiple AP credentials are received from WPS, connect with first one */
            // ESP_LOGI(TAG, "Connecting to SSID: %s, Passphrase: %s",
            //          wifi_ap_configs[0].sta.ssid, wifi_ap_configs[0].sta.password);
            ESP_LOGI(TAG, "Connecting to SSID: %s", wifi_ap_configs[0].sta.ssid);
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_ap_configs[0]));
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
        stop_blink();
        break;
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_WPS_ER_TIMEOUT");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        stop_blink();
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
    case WIFI_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
        wifi_event_ap_staconnected_t *ap_event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(ap_event->mac), ap_event->aid);
        break;
    case WIFI_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
        // wifi_event_ap_stadisconnected_t *ap_event = (wifi_event_ap_stadisconnected_t *)event_data;
        // ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
        //          MAC2STR(ap_event->mac), ap_event->aid);
        break;
    case WIFI_EVENT_SCAN_DONE:
        ESP_LOGI(TAG, "WIFI_EVENT_SCAN_DONE");
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
    // reset boot params
    s_retry_num = 0;
    new_ap_param = false;
    start_ap_select = false;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
}

esp_err_t wifi_init(void)
{
    ESP_LOGI(TAG, "Init another modules");
    nvs_init();
    config_gpio();
    init_spiffs();

    ESP_LOGI(TAG, "Start WiFi setup");
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
    if (start_ap_select)
    {
        ESP_LOGW(TAG, "Restarted as AP select mode");
    }
    else if (strlen(&wifi_config.sta.ssid) == 0 || strlen(&wifi_config.sta.password) == 0)
        start_ap_select = true;
    else
    {
        ESP_LOGI(TAG, "use ssid and password stored in nvs. SSID: %s", wifi_config.sta.ssid);
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));

    initialized_wifi = true;
    ESP_LOGI(TAG, "wifi_init finished.");

    if (start_ap_select | pressed_triger())
    {
        if (start_ap_select)
            ESP_LOGW(TAG, "No valid AP info in NVS");
        else
            ESP_LOGW(TAG, "Pressed AP select button");

        wifi_ap_select_mode();
    }

    return ESP_OK;
}

void wifi_connect(void)
{
    if (started_wifi)
    {
        ESP_LOGI(TAG, "already wifi started.");
        return;
    }

    if (!initialized_wifi)
    {
        ESP_LOGE(TAG, "wifi not initialized.");
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    started_wifi = true;
    ESP_LOGI(TAG, "wifi started.");
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

    ESP_LOGI(TAG, "get into WPS mode.");
    esp_wps_config_t wps_config = WPS_CONFIG_INIT_DEFAULT(WPS_MODE);
    ESP_ERROR_CHECK(esp_wifi_wps_enable(&wps_config));
    ESP_ERROR_CHECK(esp_wifi_wps_start(0));
}

int wifi_wait_connection(void)
{
    int connected = 0;
    /* Waiting until the connection is established (WIFI_CONNECTED_BIT).
    The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        connected = 1;
        ESP_LOGI(TAG, "Connected to AP");
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
            vTaskDelay(pdMS_TO_TICKS(2 * 1000));
        }
        vTaskDelay(pdMS_TO_TICKS(2 * 1000));
    }

    ESP_LOGI(TAG, "wifi stop.");
    wifi_stop_flag = true;
    started_wifi = false;
    // ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler));
    esp_wifi_stop();
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
}

static void print_auth_mode(int authmode)
{
    switch (authmode)
    {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

void wifi_ap_select_mode(void)
{
    ESP_LOGI(TAG, "start AP select mode.");
    ap_select_mode = true;
    esp_wifi_disconnect();

    memset(ap_info, 0, sizeof(ap_info));

    // without this statement, no IP address is provided.
    esp_netif_create_default_wifi_ap();
    // chamge mode before set config
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // filter WPA2 access point
            .pmf_cfg = {.capable = true, .required = false},
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = CONFIG_WIFI_AP_SSID,
            .ssid_len = strlen(CONFIG_WIFI_AP_SSID),
            .password = CONFIG_WIFI_AP_PASSWORD,
            .authmode = WIFI_AUTH_WPA2_PSK, // use WPA2 is password is provided
            .max_connection = CONFIG_MAX_AP_CONNECT,
        },
    };
    if (strlen(CONFIG_WIFI_AP_PASSWORD) == 0)
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    // hold until scaning all channels.
    ESP_LOGI(TAG, "AP scan start");
    esp_wifi_scan_start(NULL, true);
    ESP_LOGI(TAG, "AP scan finished");

    ap_count = CONFIG_SCAN_AP_LIST_SIZE;
    // tell max size by ap_count arg and get actual number in it.
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_info));

    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    for (int i = 0; i < ap_count; i++)
    {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        print_auth_mode(ap_info[i].authmode);
        ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
    }

    // ready to select
    start_blink();

    restart_semaphore = xSemaphoreCreateBinary();

    ESP_LOGI(TAG, "Start mDNS service");
    ns_start_mdns();
    ns_add_mdns("_http", "_tcp", 80);

    ESP_LOGI(TAG, "Start HTTP server");
    httpd_handle_t server = start_ap_select_web();

    // wait until restart
    ESP_LOGI(TAG, "Wait AP select ...");
    xSemaphoreTake(restart_semaphore, portMAX_DELAY);
    ESP_LOGI(TAG, "Selected AP");
    new_ap_param = true;

    vTaskDelay(pdMS_TO_TICKS(1000));
    if (server)
    {
        ESP_LOGI(TAG, "Stop HTTP server");
        stop_ap_select_web(server);
    }
    ESP_LOGI(TAG, "Stop mDNS service");
    ns_stop_mdns();

    wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_ap_configs[0]));
    ap_select_mode = false;
    stop_blink();
}

void wifi_set_ap(char *ssid, char *password)
{
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // filter WPA2 access point
            .pmf_cfg = {.capable = true, .required = false},
        },
    };
    memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, password, sizeof(wifi_config.sta.ssid));
    if (strlen(password) == 0)
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_ap_configs[0] = wifi_config;
    // ESP_LOGI(TAG, "Connect to SSID: %s, Passphrase: %s",
    //          wifi_ap_configs[0].sta.ssid, wifi_ap_configs[0].sta.password);
    ESP_LOGI(TAG, "Connect to SSID: %s", wifi_ap_configs[0].sta.ssid);

    xSemaphoreGive(restart_semaphore);
}

wifi_ap_record_t *wifi_get_ap_info(uint16_t *info_size)
{
    *info_size = (ap_count < CONFIG_SCAN_AP_LIST_SIZE) ? ap_count : CONFIG_SCAN_AP_LIST_SIZE;

    return ap_info;
}
