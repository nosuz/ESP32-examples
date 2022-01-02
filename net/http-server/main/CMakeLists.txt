idf_component_register(SRCS "http-server.c"
                    "gpio.c" "m_dns.c" "spiffs.c"
                    "nvs.c" "wifi.c" "http_server.c"
                    INCLUDE_DIRS ".")

set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../www")
spiffs_create_partition_image(www ${WEB_SRC_DIR} FLASH_IN_PROJECT)
