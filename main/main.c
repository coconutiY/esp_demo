/*
 * SPDX-FileCopyrightText: 2010-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bt_prov.h"
#include "camera_stream.h"
#include "mqtt_client_app.h"
#include "rgb_led.h"
#include "temperature_dht11.h"
#include "sound_detect.h"
#include "ir_remote.h"
#include "hc_sr04.h"

static const char *TAG = "APP";

static void wifi_connected_task(void *arg)
{
    char *ip_str = (char *)arg;

    http_server_start();
    if (mqtt_app_start_from_nvs() == ESP_OK) {
        ESP_LOGI(TAG, "MQTT auto-started from NVS");
    } else {
        ESP_LOGI(TAG, "No MQTT broker in NVS (visit /mqtt to configure)");
    }
    mqtt_app_publish_device_status(ip_str);
    dht11_start(30);
    sound_detect_start();
    ir_remote_start();
    hc_sr04_start(2);

    free(ip_str);
    vTaskDelete(NULL);
}

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
        char *ip_copy = strdup(info ? info : "");
        if (ip_copy) {
            xTaskCreate(wifi_connected_task, "wifi_setup", 6144, ip_copy, 5, NULL);
        }
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

    rgb_led_init();

    err = bt_prov_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bt_prov_start failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Bluetooth provisioning ready. Connect with BLE client and write SSID/password.");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
