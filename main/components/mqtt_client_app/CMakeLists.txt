idf_component_register(SRCS "src/mqtt_client_app.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES lwip log nvs_flash freertos esp_netif mqtt)
