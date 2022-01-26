idf_component_register(
    SRCS "ns_wifi.c"
    "ns_gpio.c" "ns_nvs.c"
    INCLUDE_DIRS "include"
    REQUIRES wpa_supplicant nvs_flash
)
