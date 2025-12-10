
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "wifi_provision.h"

#define WIFI_SSID "REDMI K80"
#define WIFI_PASS "1902Pan."
#define MAX_RETRY 5

static const char *TAG = "Main-App";
const int WIFI_CONNECTED_BIT = BIT0;


void app_main(void)
{
    esp_err_t rc = wifi_provision_sta(WIFI_SSID, WIFI_PASS, 30);
    if (rc != ESP_OK)
    {
        ESP_LOGE(TAG, "provision fail, reboot");
        esp_restart();
    }
}