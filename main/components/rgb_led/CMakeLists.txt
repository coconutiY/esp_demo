idf_component_register(SRCS "src/rgb_led.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_ledc log)
