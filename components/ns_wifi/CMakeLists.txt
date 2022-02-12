idf_component_register(
    SRCS "ns_wifi.c"
    "ns_gpio.c" "ns_nvs.c" "ns_ap_select_web.c"
    INCLUDE_DIRS "include"
    REQUIRES wpa_supplicant nvs_flash esp_http_server ns_mdns
)

set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ap-select-app")
if(EXISTS ${WEB_SRC_DIR}/dist)
    spiffs_create_partition_image(www ${WEB_SRC_DIR}/dist FLASH_IN_PROJECT)
else()
    message(FATAL_ERROR "${WEB_SRC_DIR}/dist doesn't exit. Please run 'npm run build' in ${WEB_SRC_DIR}")
endif()
