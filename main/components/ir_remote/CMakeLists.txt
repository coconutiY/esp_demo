idf_component_register(SRCS "src/ir_remote.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_driver_rmt log freertos mqtt_client_app)
