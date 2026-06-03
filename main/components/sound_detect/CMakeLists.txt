idf_component_register(SRCS "src/sound_detect.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_driver_gpio log freertos mqtt_client_app rgb_led)
