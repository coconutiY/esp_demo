idf_component_register(SRCS "src/mqtt_client_app.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES espressif__esp-mqtt log nvs_flash freertos)
