#include "blufi_mgr.h"
#include "esp_wifi.h"
#include "esp_blufi.h"
#include "esp_blufi_api.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"

static const char *TAG = "blufi_mgr";

/* ---------- BluFi 回调 ---------- */
static void blufi_event_handler(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        ESP_LOGI(TAG, "BluFi init done");
        esp_blufi_profile_init();
        break;

    case ESP_BLUFI_EVENT_SET_WIFI_CONFIG:
        /* 手机发下来 SSID/密码 */
        wifi_config_t wifi_cfg = { 0 };
        memcpy(wifi_cfg.sta.ssid, param->wifi_config.sta.ssid, param->wifi_config.sta.ssid_len);
        memcpy(wifi_cfg.sta.password, param->wifi_config.sta.password, param->wifi_config.sta.password_len);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
        ESP_LOGI(TAG, "Wi-Fi config received, SSID:%s", wifi_cfg.sta.ssid);
        esp_wifi_connect();   // 立即连接
        break;

    case ESP_BLUFI_EVENT_STA_CONNECT:
        esp_wifi_connect();   // App 点击 Connect
        break;

    case ESP_BLUFI_EVENT_STA_DISCONNECT:
        esp_wifi_disconnect();// App 点击 Disconnect
        break;

    default:
        break;
    }
}

/* ---------- Wi-Fi 事件 ---------- */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected");
        esp_wifi_connect();               // 自动重连
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&e->ip_info.ip));
        /* 可选：通知 App 连接成功 */
        esp_blufi_send_wifi_conn_report(WIFI_MODE_STA, ESP_BLUFI_STA_CONN_SUCCESS, 0, NULL);
    }
}

/* ---------- 对外接口 ---------- */
esp_err_t blufi_mgr_start(void)
{
    /* 1. NVS / netif / 事件循环 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    /* 2. Wi-Fi 驱动 */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* 3. BLE + BluFi */
    esp_blufi_register_callbacks(blufi_event_handler);
    ESP_ERROR_CHECK(esp_blufi_profile_init());
    ESP_LOGI(TAG, "BluFi ready, device name: ESP32-S3-BluFi");
    return ESP_OK;
}