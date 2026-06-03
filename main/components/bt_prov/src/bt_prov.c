#include "bt_prov.h"
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_defs.h"

static const char *TAG = "bt_prov";
#define DEFAULT_DEVICE_NAME "ESP32_PROVISION"

#define BT_PROV_NVS_NS    "bt_prov"
#define BT_PROV_NVS_SSID  "ssid"
#define BT_PROV_NVS_PASS  "pass"

#define GATTS_SERVICE_UUID_PROV   0x00FF
#define GATTS_CHAR_UUID_SSID      0xFF01
#define GATTS_CHAR_UUID_PASS      0xFF02
#define GATTS_CHAR_UUID_STATUS    0xFF03
#define GATTS_NUM_HANDLE          IDX_NB
#define MAX_SSID_LEN              32
#define MAX_PASS_LEN              64
#define MAX_STATUS_LEN            32

enum {
    IDX_SVC,
    IDX_CHAR_SSID,
    IDX_CHAR_VAL_SSID,
    IDX_CHAR_PASS,
    IDX_CHAR_VAL_PASS,
    IDX_CHAR_STATUS,
    IDX_CHAR_VAL_STATUS,
    IDX_NB,
};

static bt_prov_config_t s_config = {
    .device_name = DEFAULT_DEVICE_NAME,
    .event_cb = NULL,
};

static bool s_initialized = false;
static bool s_wifi_started = false;
static bool s_wifi_connected = false;
static bool s_wifi_connecting = false;
static bool s_start_requested = false;
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_handle_table[GATTS_NUM_HANDLE] = {0};
static char s_ssid[MAX_SSID_LEN + 1] = {0};
static char s_password[MAX_PASS_LEN + 1] = {0};
static char s_status[MAX_STATUS_LEN + 1] = "idle";

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t service_uuid          = GATTS_SERVICE_UUID_PROV;
static const uint16_t char_decl_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t char_uuid_ssid = GATTS_CHAR_UUID_SSID;
static const uint16_t char_uuid_pass = GATTS_CHAR_UUID_PASS;
static const uint16_t char_uuid_status = GATTS_CHAR_UUID_STATUS;
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_gatts_attr_db_t gatt_db[IDX_NB] = {
    [IDX_SVC] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
          sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&service_uuid}},

    [IDX_CHAR_SSID] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid, ESP_GATT_PERM_READ,
          sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_write}},

    [IDX_CHAR_VAL_SSID] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&char_uuid_ssid, ESP_GATT_PERM_WRITE,
          MAX_SSID_LEN, 0, NULL}},

    [IDX_CHAR_PASS] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid, ESP_GATT_PERM_READ,
          sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_write}},

    [IDX_CHAR_VAL_PASS] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&char_uuid_pass, ESP_GATT_PERM_WRITE,
          MAX_PASS_LEN, 0, NULL}},

    [IDX_CHAR_STATUS] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid, ESP_GATT_PERM_READ,
          sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read}},

    [IDX_CHAR_VAL_STATUS] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&char_uuid_status, ESP_GATT_PERM_READ,
          MAX_STATUS_LEN, sizeof(s_status), (uint8_t *)s_status}},
};

static esp_err_t bt_prov_load_creds(char *ssid, size_t ssid_sz, char *pass, size_t pass_sz)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(BT_PROV_NVS_NS, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    size_t sl = ssid_sz, pl = pass_sz;
    err = nvs_get_str(handle, BT_PROV_NVS_SSID, ssid, &sl);
    if (err == ESP_OK) {
        err = nvs_get_str(handle, BT_PROV_NVS_PASS, pass, &pl);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            /* SSID 存在但密码缺失（开放网络）— 视为空密码 */
            pass[0] = '\0';
            err = ESP_OK;
        }
    }
    nvs_close(handle);
    return err;
}

static esp_err_t bt_prov_save_creds(const char *ssid, const char *pass)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(BT_PROV_NVS_NS, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(handle, BT_PROV_NVS_SSID, ssid ? ssid : "");
    if (err == ESP_OK) {
        err = nvs_set_str(handle, BT_PROV_NVS_PASS, pass ? pass : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t bt_prov_erase_creds(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(BT_PROV_NVS_NS, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    nvs_erase_key(handle, BT_PROV_NVS_SSID);
    nvs_erase_key(handle, BT_PROV_NVS_PASS);
    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static void bt_prov_send_event(bt_prov_event_t event, esp_err_t err, const char *info)
{
    if (s_config.event_cb) {
        s_config.event_cb(event, err, info);
    }
}

static void bt_prov_update_status(const char *status)
{
    if (!status) {
        return;
    }
    strncpy(s_status, status, sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';

    if (s_handle_table[IDX_CHAR_VAL_STATUS]) {
        esp_ble_gatts_set_attr_value(s_handle_table[IDX_CHAR_VAL_STATUS], strlen(s_status), (uint8_t *)s_status);
    }
}

static void bt_prov_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                       int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "Wi-Fi STA started");
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            s_wifi_connected = false;
            s_wifi_connecting = false;
            ESP_LOGI(TAG, "Wi-Fi disconnected, retrying");
            bt_prov_update_status("wifi disconnected");
            bt_prov_send_event(BT_PROV_EVENT_WIFI_DISCONNECTED, ESP_FAIL, NULL);
            if (s_ssid[0]) {
                wifi_config_t wifi_config = {
                    .sta = {
                        .ssid = "",
                        .password = "",
                        .pmf_cfg = {.capable = true, .required = false},
                    },
                };
                strncpy((char *)wifi_config.sta.ssid, s_ssid, sizeof(wifi_config.sta.ssid) - 1);
                strncpy((char *)wifi_config.sta.password, s_password, sizeof(wifi_config.sta.password) - 1);
                esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
                esp_wifi_connect();
                s_wifi_connecting = true;
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *got_ip = (ip_event_got_ip_t *)event_data;
        s_wifi_connected = true;
        s_wifi_connecting = false;
        char ip_str[32];
        esp_ip4addr_ntoa(&got_ip->ip_info.ip, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "Wi-Fi connected, IP: %s", ip_str);

        esp_err_t save_err = bt_prov_save_creds(s_ssid, s_password);
        if (save_err != ESP_OK) {
            ESP_LOGW(TAG, "Save credentials to NVS failed: %s", esp_err_to_name(save_err));
        }

        bt_prov_update_status("wifi connected");
        bt_prov_send_event(BT_PROV_EVENT_WIFI_CONNECTED, ESP_OK, ip_str);
        bt_prov_send_event(BT_PROV_EVENT_PROVISION_COMPLETE, ESP_OK, s_ssid);
    }
}

static esp_err_t bt_prov_wifi_init(void)
{
    if (s_wifi_started) {
        return ESP_OK;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_init_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &bt_prov_wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &bt_prov_wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }

    s_wifi_started = true;
    return ESP_OK;
}

static void bt_prov_try_connect(void)
{
    if (!s_ssid[0]) {
        return;
    }

    esp_err_t err = bt_prov_wifi_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi init failed: %s", esp_err_to_name(err));
        bt_prov_update_status("wifi init failed");
        bt_prov_send_event(BT_PROV_EVENT_ERROR, err, "wifi init failed");
        return;
    }

    if (s_wifi_connecting || s_wifi_connected) {
        /* 新凭据写入时正在连接：先断开，断开事件处理器会用最新凭据重连 */
        esp_wifi_disconnect();
        s_wifi_connecting = false;
        s_wifi_connected = false;
        bt_prov_update_status("wifi reconnecting");
        return;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };

    strncpy((char *)wifi_config.sta.ssid, s_ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, s_password, sizeof(wifi_config.sta.password) - 1);

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
        bt_prov_send_event(BT_PROV_EVENT_ERROR, err, "set config failed");
        return;
    }

    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
        bt_prov_update_status("connect failed");
        bt_prov_send_event(BT_PROV_EVENT_ERROR, err, "connect failed");
        return;
    }

    s_wifi_connecting = true;
    bt_prov_update_status("wifi connecting");
    bt_prov_send_event(BT_PROV_EVENT_WIFI_CONNECTING, ESP_OK, s_ssid);
}

static void bt_prov_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "BLE advertising started");
            bt_prov_update_status("ble advertising");
            bt_prov_send_event(BT_PROV_EVENT_BLE_ADV_STARTED, ESP_OK, NULL);
        } else {
            ESP_LOGE(TAG, "BLE advertising failed to start");
            bt_prov_update_status("advertise failed");
            bt_prov_send_event(BT_PROV_EVENT_ERROR, ESP_FAIL, "adv start failed");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "BLE advertising stopped");
        break;
    default:
        break;
    }
}

static void bt_prov_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                         esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        if (param->reg.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "GATTS register app failed, status %d", param->reg.status);
            bt_prov_send_event(BT_PROV_EVENT_ERROR, ESP_FAIL, "gatts register failed");
            return;
        }
        s_gatts_if = gatts_if;
        esp_ble_gap_set_device_name(s_config.device_name);

        esp_ble_adv_data_t adv_data = {
            .set_scan_rsp = false,
            .include_name = true,
            .include_txpower = true,
            .min_interval = 0x20,
            .max_interval = 0x40,
            .appearance = 0x00,
            .manufacturer_len = 0,
            .p_manufacturer_data = NULL,
            .service_data_len = 0,
            .p_service_data = NULL,
            .service_uuid_len = sizeof(service_uuid),
            .p_service_uuid = (uint8_t *)&service_uuid,
            .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
        };

        esp_ble_gap_config_adv_data(&adv_data);
        esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, IDX_NB, 0);
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            bt_prov_send_event(BT_PROV_EVENT_ERROR, ESP_FAIL, "create attr table failed");
            return;
        }
        memcpy(s_handle_table, param->add_attr_tab.handles, sizeof(s_handle_table));
        esp_ble_gatts_start_service(s_handle_table[IDX_SVC]);
        break;

    case ESP_GATTS_WRITE_EVT:
        if (param->write.need_rsp) {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                        ESP_GATT_OK, NULL);
        }
        if (param->write.handle == s_handle_table[IDX_CHAR_VAL_SSID]) {
            size_t len = param->write.len;
            if (len > MAX_SSID_LEN) {
                len = MAX_SSID_LEN;
            }
            memcpy(s_ssid, param->write.value, len);
            s_ssid[len] = '\0';
            ESP_LOGI(TAG, "received SSID: %s", s_ssid);
            bt_prov_try_connect();
        } else if (param->write.handle == s_handle_table[IDX_CHAR_VAL_PASS]) {
            size_t len = param->write.len;
            if (len > MAX_PASS_LEN) {
                len = MAX_PASS_LEN;
            }
            memcpy(s_password, param->write.value, len);
            s_password[len] = '\0';
            ESP_LOGI(TAG, "received password length: %d", (int)len);
            bt_prov_try_connect();
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "BLE client connected");
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "BLE client disconnected");
        esp_ble_gap_start_advertising(&adv_params);
        break;

    default:
        break;
    }
}

esp_err_t bt_prov_init(const bt_prov_config_t *config)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (config) {
        if (config->device_name) {
            s_config.device_name = config->device_name;
        }
        s_config.event_cb = config->event_cb;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK) {
        return err;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    err = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_bluedroid_enable();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_ble_gap_register_callback(bt_prov_gap_event_handler);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_ble_gatts_register_callback(bt_prov_gatts_event_handler);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_ble_gatts_app_register(0);
    if (err != ESP_OK) {
        return err;
    }

    s_initialized = true;
    bt_prov_update_status("initialized");
    bt_prov_send_event(BT_PROV_EVENT_STARTED, ESP_OK, NULL);
    s_start_requested = true;

    /* 尝试加载已保存的凭据并自动重连 */
    char stored_ssid[MAX_SSID_LEN + 1] = {0};
    char stored_pass[MAX_PASS_LEN + 1] = {0};
    if (bt_prov_load_creds(stored_ssid, sizeof(stored_ssid), stored_pass, sizeof(stored_pass)) == ESP_OK
            && stored_ssid[0] != '\0') {
        ESP_LOGI(TAG, "Found stored credentials, auto-connecting to SSID: %s", stored_ssid);
        strncpy(s_ssid, stored_ssid, sizeof(s_ssid) - 1);
        s_ssid[sizeof(s_ssid) - 1] = '\0';
        strncpy(s_password, stored_pass, sizeof(s_password) - 1);
        s_password[sizeof(s_password) - 1] = '\0';
        bt_prov_try_connect();
    } else {
        ESP_LOGI(TAG, "No stored credentials, waiting for BLE provisioning");
    }

    return ESP_OK;
}

esp_err_t bt_prov_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    s_start_requested = true;
    if (s_handle_table[IDX_SVC]) {
        return esp_ble_gap_start_advertising(&adv_params);
    }
    return ESP_OK;
}

esp_err_t bt_prov_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    s_start_requested = false;
    return esp_ble_gap_stop_advertising();
}

esp_err_t bt_prov_clear_credentials(void)
{
    s_ssid[0] = '\0';
    s_password[0] = '\0';
    if (s_wifi_started) {
        esp_wifi_disconnect();
    }
    return bt_prov_erase_creds();
}
