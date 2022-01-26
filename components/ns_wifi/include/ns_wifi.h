#include "ns_nvs.h"
#include "ns_gpio.h"

int wifi_init(void);
void wifi_connect(void);
void wifi_wps_start(void);
int wifi_wait_connection(void);
void wifi_disconnect(void);
