#include "mqtt_client_app.h"
#include "rgb_led.h"
#include "temperature_dht11.h"
#include "sound_detect.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_event.h"

static const char *TAG = "mqtt_app";

#define TOPIC_STATUS  "esp32s3/status"
#define TOPIC_CONTROL "esp32s3/control"
#define NVS_NS        "mqtt_cfg"
#define NVS_KEY_URI   "broker_uri"
#define NVS_KEY_EN    "enabled"

static esp_mqtt_client_handle_t s_client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected!");
        esp_mqtt_client_subscribe(client, TOPIC_CONTROL, 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribed to control topic");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Published");
        break;
    case MQTT_EVENT_DATA: {
        ESP_LOGI(TAG, "Control: %.*s", event->data_len, event->data);
        int dlen = event->data_len;
        const char *d = event->data;
        if (dlen == 8 && strncmp(d, "sound_on", 8) == 0) {
            sound_detect_enable(true);
        } else if (dlen == 9 && strncmp(d, "sound_off", 9) == 0) {
            sound_detect_enable(false);
        } else if (dlen == 15 && strncmp(d, "temperature_get", 15) == 0) {
            dht11_data_t data;
            esp_err_t ret = dht11_read(&data);
            char msg[128];
            if (ret == ESP_OK) {
                snprintf(msg, sizeof(msg),
                         "{\"type\":\"sensor\",\"event\":\"dht11\",\"data\":"
                         "{\"temperature\":%.1f,\"humidity\":%.1f}}",
                         data.temperature, data.humidity);
            } else {
                snprintf(msg, sizeof(msg),
                         "{\"type\":\"sensor\",\"event\":\"dht11_error\",\"data\":"
                         "{\"error\":\"%s\"}}",
                         esp_err_to_name(ret));
            }
            mqtt_app_publish_status(msg);
        } else {
            rgb_led_handle_command(d, dlen);
        }
        break;
    }
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;
    default:
        break;
    }
}

static bool is_connected(void)
{
    return s_client != NULL;
}

esp_err_t mqtt_app_init(const char *broker_uri)
{
    if (!broker_uri || broker_uri[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_client) {
        mqtt_app_stop();
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    return ESP_OK;
}

esp_err_t mqtt_app_start_from_nvs(void)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (ret != ESP_OK) return ret;

    uint8_t en = 0;
    if (nvs_get_u8(h, NVS_KEY_EN, &en) != ESP_OK || en == 0) {
        nvs_close(h);
        return ESP_ERR_NOT_FOUND;
    }

    char broker_uri[128] = {0};
    size_t len = sizeof(broker_uri);
    ret = nvs_get_str(h, NVS_KEY_URI, broker_uri, &len);
    nvs_close(h);
    if (ret != ESP_OK || broker_uri[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }

    return mqtt_app_init(broker_uri);
}

esp_err_t mqtt_app_configure(const char *broker_uri)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_str(h, NVS_KEY_URI, broker_uri ? broker_uri : "");
    if (ret == ESP_OK) {
        uint8_t en = (broker_uri && broker_uri[0]) ? 1 : 0;
        ret = nvs_set_u8(h, NVS_KEY_EN, en);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(h);
    }
    nvs_close(h);
    if (ret != ESP_OK) return ret;

    mqtt_app_stop();
    if (broker_uri && broker_uri[0]) {
        vTaskDelay(pdMS_TO_TICKS(200));
        return mqtt_app_init(broker_uri);
    }
    return ESP_OK;
}

void mqtt_app_publish_status(const char *msg)
{
    if (!is_connected() || !msg) return;
    esp_mqtt_client_publish(s_client, TOPIC_STATUS, msg, strlen(msg), 0, 0);
}

void mqtt_app_publish_device_status(const char *ip)
{
    if (!is_connected()) return;
    char buf[160];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"device\",\"event\":\"connected\",\"data\":{\"ip\":\"%s\"}}",
             ip ? ip : "");
    mqtt_app_publish_status(buf);
}

void mqtt_app_publish_camera_status(bool ok)
{
    if (!is_connected()) return;
    char buf[96];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"camera\",\"event\":\"%s\",\"data\":{}}",
             ok ? "ok" : "fail");
    mqtt_app_publish_status(buf);
}

void mqtt_app_stop(void)
{
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
}