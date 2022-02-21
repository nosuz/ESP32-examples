#include "esp_wifi.h"

#include "ns_mdns.h"

esp_err_t wifi_init(void);
void wifi_connect(void);
int wifi_wait_connection(void);
void wifi_disconnect(void);
void wifi_ap_select_mode(void);
void wifi_set_ap(char *ssid, char *passwd);
wifi_ap_record_t *wifi_get_ap_info(uint16_t *info_size);
