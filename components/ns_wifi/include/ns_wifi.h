#include "ns_nvs.h"
#include "ns_gpio.h"
#include "ns_ap_select_web.h"
#include "ns_mdns.h"

int wifi_init(void);
void wifi_connect(void);
// void wifi_wps_start(void);
// void wifi_start_ap_select(void);
int wifi_wait_connection(void);
void wifi_disconnect(void);
void wifi_ap_select_mode(void);
void wifi_set_ap(char *ssid, char *passwd);
