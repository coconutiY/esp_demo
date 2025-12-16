idf_component_register(SRCS "app_main.c"
        PRIV_REQUIRES wifi_provision blufi_mgr camera_detect esp_camera
        INCLUDE_DIRS ".")
