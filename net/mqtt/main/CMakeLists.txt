idf_component_register(
    SRCS "mqtt_main.c"
    "gpio.c" "nvs.c" "wifi.c" "sntp.c"
    "spi.c" "adt7310.c"
    "mqtt.c"
    INCLUDE_DIRS "."
)
