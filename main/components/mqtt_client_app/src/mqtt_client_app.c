#include "mqtt_client_app.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

#define TOPIC_DEVICE  "esp32s3/status/device"
#define TOPIC_CAMERA  "esp32s3/status/camera"
#define TOPIC_CONTROL "esp32s3/control"
#define NVS_NS        "mqtt_cfg"
#define NVS_KEY       "broker_uri"

static const char *TAG = "mqtt_app";
static esp_mqtt_client_handle_t s_client = NULL;

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to broker");
        esp_mqtt_client_subscribe(s_client, TOPIC_CONTROL, 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from broker");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Control: topic=%.*s data=%.*s",
                 event->topic_len, event->topic,
                 event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;
    default:
        break;
    }
}

esp_err_t mqtt_app_init(const char *broker_uri)
{
    if (s_client) {
        return ESP_OK;
    }
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = broker_uri,
    };
    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        return ESP_FAIL;
    }
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(s_client);
}

esp_err_t mqtt_app_start_from_nvs(void)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    char uri[128] = {0};
    size_t len = sizeof(uri);
    ret = nvs_get_str(h, NVS_KEY, uri, &len);
    nvs_close(h);
    if (ret != ESP_OK || uri[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }
    return mqtt_app_init(uri);
}

esp_err_t mqtt_app_configure(const char *broker_uri)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_str(h, NVS_KEY, broker_uri);
    if (ret == ESP_OK) nvs_commit(h);
    nvs_close(h);
    if (ret != ESP_OK) return ret;

    mqtt_app_stop();
    vTaskDelay(pdMS_TO_TICKS(200));
    return mqtt_app_init(broker_uri);
}

void mqtt_app_publish_device_status(const char *ip)
{
    if (!s_client) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"ip\":\"%s\",\"status\":\"connected\"}", ip ? ip : "");
    esp_mqtt_client_publish(s_client, TOPIC_DEVICE, buf, 0, 1, 0);
}

void mqtt_app_publish_camera_status(bool ok)
{
    if (!s_client) return;
    const char *payload = ok ? "{\"camera\":\"ok\"}" : "{\"camera\":\"fail\"}";
    esp_mqtt_client_publish(s_client, TOPIC_CAMERA, payload, 0, 1, 0);
}

void mqtt_app_stop(void)
{
    if (!s_client) return;
    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client = NULL;
}
