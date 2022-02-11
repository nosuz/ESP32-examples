#include <stdio.h>
#include <mdns.h>

#include "esp_err.h"
#include "esp_log.h"

#include "ns_mdns.h"

#define MDNS_HOSTNAME CONFIG_MDNS_HOSTNAME
#define MDNS_INSTANCE CONFIG_MDNS_INSTANCE

static const char *TAG = "mdns";

void ns_start_mdns(void)
{
    esp_err_t err = mdns_init();
    if (err)
    {
        printf("MDNS Init failed: %d\n", err);
        return;
    }

    ESP_LOGI(TAG, "mDNS: %s (%s)", MDNS_HOSTNAME, MDNS_INSTANCE);
    mdns_hostname_set(MDNS_HOSTNAME);
    // mdns_instance_name_set(MDNS_INSTANCE);
}

void ns_add_mdns(char *service_type, char *proto, uint16_t port)
{

    // "_http", "_tcp", 80
    mdns_service_add(
        MDNS_INSTANCE,
        service_type,
        proto, port, NULL, 0);
}

void ns_stop_mdns(void)
{
    mdns_free();
}
