#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t mqtt_app_init(const char *broker_uri);
esp_err_t mqtt_app_start_from_nvs(void);
esp_err_t mqtt_app_configure(const char *broker_uri);
void mqtt_app_publish_device_status(const char *ip);
void mqtt_app_publish_camera_status(bool ok);
void mqtt_app_stop(void);
