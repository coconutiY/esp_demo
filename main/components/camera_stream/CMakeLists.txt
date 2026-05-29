idf_component_register(SRCS "src/camera_stream.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES espressif__esp32-camera esp_http_server log
                                     esp_driver_ledc esp_driver_i2c esp_driver_gpio
                                     mqtt_client_app face_detect mbedtls)
