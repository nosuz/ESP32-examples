#include "esp_wifi.h"

#include "ns_nvs.h"
#include "ns_gpio.h"
#include "ns_spiffs.h"
#include "ns_ap_select_web.h"
#include "ns_mdns.h"

esp_err_t wifi_init(void);
void wifi_connect(void);
// void wifi_wps_start(void);
// void wifi_start_ap_select(void);
int wifi_wait_connection(void);
void wifi_disconnect(void);
void wifi_ap_select_mode(void);
void wifi_set_ap(char *ssid, char *passwd);
wifi_ap_record_t *wifi_get_ap_info(uint16_t *info_size);
