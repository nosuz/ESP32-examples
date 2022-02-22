idf_component_register(
    SRCS "ns_ambient.c"
    INCLUDE_DIRS "include"
    REQUIRES esp-tls esp_http_client json
)
