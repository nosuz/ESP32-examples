idf_component_register(
    SRCS "ns_google_api.c"
    INCLUDE_DIRS "include"
    REQUIRES json nvs_flash ns_sntp ns_http
    )
