idf_component_register(SRCS "https_client_main.c"
                    "gpio.c" "nvs.c" "wifi.c"
                    "https_client.c"
                    # EMBED_TXTFILES www_nosuz_jp.pem
                    INCLUDE_DIRS ".")
