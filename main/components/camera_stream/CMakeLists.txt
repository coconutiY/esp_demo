idf_component_register(SRCS "src/camera_stream.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp32_camera esp_http_server log)
