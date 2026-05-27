idf_component_register(SRCS "src/bt_prov.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES bt nvs_flash esp_netif esp_wifi esp_event)
