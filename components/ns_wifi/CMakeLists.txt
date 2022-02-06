idf_component_register(
    SRCS "ns_wifi.c"
    "ns_gpio.c" "ns_nvs.c" "ns_ap_select_web.c"
    INCLUDE_DIRS "include"
    REQUIRES wpa_supplicant nvs_flash esp_http_server
)