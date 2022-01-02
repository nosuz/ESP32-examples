#include "esp_err.h"
#include "esp_log.h"
#include <mdns.h>

#define MDNS_HOSTNAME CONFIG_MDNS_HOSTNAME
#define MDNS_INSTANCE CONFIG_MDNS_INSTANCE

static const char *TAG = "mdns";

void start_mdns_service(void)
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

    mdns_service_add(MDNS_INSTANCE,
                     "_http",
                     "_tcp", 80, NULL, 0);
}
