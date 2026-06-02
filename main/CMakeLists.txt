idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash bt_prov camera_stream esp_psram mqtt_client_app rgb_led
                       INCLUDE_DIRS "")
