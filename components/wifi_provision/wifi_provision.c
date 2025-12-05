/**
 * @file   wifi_provision.c
 * @brief  Wi-Fi Station 阻塞式连接实现
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "wifi_provision.h"
#define MAX_RETRY 5        /* 内部重试次数 */
#define CONNECTED_BIT BIT0 /* 事件组位 */

static const char *TAG = "wifi_provision";
static EventGroupHandle_t s_wifi_event_group; /* 内部事件组 */
static int s_retry_num = 0;                   /* 当前重试计数 */

/* 内部事件回调：仅用于置位/重试 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *ev = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "Wi-Fi disconnected, reason=%d", ev->reason);

        if (s_retry_num < MAX_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect (%d/%d)", s_retry_num, MAX_RETRY);
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT); /* 通知外部失败 */
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT); /* 通知外部成功 */
    }
}

esp_err_t wifi_provision_sta(const char *ssid, const char *pass, int timeout_sec)
{
    if (ssid == NULL)
    {
        ESP_LOGE(TAG, "SSID is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* 1. 首次调用时初始化 NVS / netif / 事件循环 */
    static bool s_inited = false;
    if (!s_inited)
    {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        s_inited = true;
    }

    /* 2. 创建事件组（每次连接都新分配，保证超时干净） */
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL)
    {
        ESP_LOGE(TAG, "No memory for event group");
        return ESP_ERR_NO_MEM;
    }

    /* 3. 创建默认 STA 网卡 */
    esp_netif_create_default_wifi_sta();

    /* 4. 注册事件 */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, &instance_got_ip));

    /* 5. Wi-Fi 驱动初始化 */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 6. 配置 STA 参数 */
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass)
    {
        strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    /* 7. 启动 + 连接 */
    ESP_ERROR_CHECK(esp_wifi_start());
    s_retry_num = 0;
    ESP_LOGI(TAG, "Wi-Fi start, connecting to \"%s\"...", ssid);
    ESP_ERROR_CHECK(esp_wifi_connect());

    /* 8. 阻塞等待成功 or 超时 */
    TickType_t timeout_ticks = (timeout_sec <= 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_sec * 1000);
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           timeout_ticks);

    /* 9. 清理事件句柄（Wi-Fi 不 Deinit，留给后续业务用） */
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    vEventGroupDelete(s_wifi_event_group);

    /* 10. 根据 EventGroup 结果返回 */
    if (bits & CONNECTED_BIT)
    {
        /* 再二次确认是否真的拿到 IP（防止超时瞬间刚好连上）*/
        if (esp_netif_get_ip_info(esp_netif_get_default_netif(ESP_NETIF_IF_STA), &(ip_addr_t){}) == ESP_OK)
        {
            ESP_LOGI(TAG, "Wi-Fi provision SUCCESS");
            return ESP_OK;
        }
    }
    ESP_LOGE(TAG, "Wi-Fi provision FAILED (timeout or reach max retry)");
    return ESP_FAIL;
}