idf_component_register(SRCS "src/face_detect.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES espressif__esp_jpeg espressif__esp32-camera log esp_psram)
