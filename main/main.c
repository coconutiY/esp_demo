/*
 * SPDX-FileCopyrightText: 2010-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bt_prov.h"
#include "camera_stream.h"
#include "mqtt_client_app.h"

static const char *TAG = "APP";
static volatile bool s_start_camera = false;

static void prov_event_handler(bt_prov_event_t event, esp_err_t err, const char *info)
{
    switch (event) {
    case BT_PROV_EVENT_STARTED:
        ESP_LOGI(TAG, "Bluetooth provision module started");
        break;
    case BT_PROV_EVENT_BLE_ADV_STARTED:
        ESP_LOGI(TAG, "BLE advertising started");
        break;
    case BT_PROV_EVENT_WIFI_CONNECTING:
        ESP_LOGI(TAG, "Wi-Fi connecting to SSID: %s", info ? info : "");
        break;
    case BT_PROV_EVENT_WIFI_CONNECTED:
        ESP_LOGI(TAG, "Wi-Fi connected, IP: %s", info ? info : "");
        http_server_start();
        s_start_camera = true;
        /* 尝试从 NVS 加载 MQTT broker 并连接 */
        if (mqtt_app_start_from_nvs() == ESP_OK) {
            ESP_LOGI(TAG, "MQTT auto-started from NVS");
        } else {
            ESP_LOGI(TAG, "No MQTT broker in NVS (visit /mqtt to configure)");
        }
        mqtt_app_publish_device_status(info);
        break;
    case BT_PROV_EVENT_WIFI_DISCONNECTED:
        ESP_LOGW(TAG, "Wi-Fi disconnected");
        mqtt_app_stop();
        break;
    case BT_PROV_EVENT_PROVISION_COMPLETE:
        ESP_LOGI(TAG, "Provision complete, SSID: %s", info ? info : "");
        break;
    case BT_PROV_EVENT_ERROR:
        ESP_LOGE(TAG, "Provision error: %s (%s)", info ? info : "unknown", esp_err_to_name(err));
        break;
    default:
        break;
    }
}

void app_main(void)
{
    bt_prov_config_t config = {
        .device_name = "ESP32-Provision",
        .event_cb = prov_event_handler,
    };

    esp_err_t err = bt_prov_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bt_prov_init failed: %s", esp_err_to_name(err));
        return;
    }

    err = bt_prov_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bt_prov_start failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Bluetooth provisioning ready. Connect with BLE client and write SSID/password.");

    while (true) {
        if (s_start_camera) {
            s_start_camera = false;
            esp_err_t err = camera_stream_init();
            mqtt_app_publish_camera_status(err == ESP_OK);
            if (err == ESP_OK) {
                camera_stream_start();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
