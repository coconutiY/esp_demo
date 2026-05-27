#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BT_PROV_EVENT_STARTED,
    BT_PROV_EVENT_BLE_ADV_STARTED,
    BT_PROV_EVENT_WIFI_CONNECTING,
    BT_PROV_EVENT_WIFI_CONNECTED,
    BT_PROV_EVENT_WIFI_DISCONNECTED,
    BT_PROV_EVENT_PROVISION_COMPLETE,
    BT_PROV_EVENT_ERROR,
} bt_prov_event_t;

typedef void (*bt_prov_event_cb_t)(bt_prov_event_t event, esp_err_t err, const char *info);

typedef struct {
    const char *device_name;
    bt_prov_event_cb_t event_cb;
} bt_prov_config_t;

esp_err_t bt_prov_init(const bt_prov_config_t *config);
esp_err_t bt_prov_start(void);
esp_err_t bt_prov_stop(void);

#ifdef __cplusplus
}
#endif
