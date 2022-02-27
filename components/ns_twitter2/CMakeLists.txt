idf_component_register(
    SRCS "ns_twitter2.c"
    INCLUDE_DIRS "include"
    REQUIRES json nvs_flash ns_sntp ns_http
    )
