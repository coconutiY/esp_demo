
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define WIFI_SSID "REDMI K80"
#define WIFI_PASS "1902Pan."
#define MAX_RETRY 5

static const char *TAG = "S3_WIFI_STA";
static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static int s_retry_num = 0;

/* 事件回调 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        /* 取出断开原因并打印 */
        wifi_event_sta_disconnected_t *ev =
            (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGE(TAG, "Wi-Fi 断开原因: %d (%s)", ev->reason,
                 esp_err_to_name(ev->reason));

        if (s_retry_num < MAX_RETRY)
        {
            ESP_LOGI(TAG, "第 %d 次重连...", s_retry_num + 1);
            esp_wifi_connect();
            s_retry_num++;
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            ESP_LOGE(TAG, "已达最大重试次数，连接失败！");
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "获取到 IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_sta_start(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "wifi_init_sta finished.");
    xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);
}

void app_main(void)
{
    esp_err_t rc = wifi_provision_sta("Readme K80", "1902Pan.", 30);
    if (rc != ESP_OK)
    {
        ESP_LOGE(TAG, "provision fail, reboot");
        esp_restart();
    }
}