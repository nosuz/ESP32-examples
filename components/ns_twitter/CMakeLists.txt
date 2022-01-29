idf_component_register(
    SRCS "ns_twitter.c"
    INCLUDE_DIRS "include"
    REQUIRES mbedtls esp-tls esp_http_client ns_sntp
    )
