idf_component_register(
    SRCS "src/hc_sr04.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_psram freertos mqtt_client_app
)
