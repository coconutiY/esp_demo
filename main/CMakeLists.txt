idf_component_register(SRCS "connect_wifi_main.c"
        PRIV_REQUIRES esp_wifi esp_netif nvs_flash esp_event
                    INCLUDE_DIRS ".")
