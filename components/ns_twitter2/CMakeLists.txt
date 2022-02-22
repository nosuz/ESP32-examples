idf_component_register(
    SRCS "ns_twitter2.c"
    INCLUDE_DIRS "include"
    REQUIRES esp-tls esp_http_client json nvs_flash ns_sntp
    )
