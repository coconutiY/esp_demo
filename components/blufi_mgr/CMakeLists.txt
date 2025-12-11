idf_component_register(
        SRCS "blufi_mgr.c"
        INCLUDE_DIRS "include"
        REQUIRES esp_wifi esp_blufi nvs_flash
)