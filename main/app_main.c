
#include <nvs.h>
#include <nvs_flash.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "wifi_provision.h"
#include "camera_detect.h"
#include "esp_camera.h"

#define WIFI_SSID "REDMI K80"
#define WIFI_PASS "1902Pan."
#define MAX_RETRY 5

static const char *TAG = "Main-App";
const int WIFI_CONNECTED_BIT = BIT0;

void business_task(void *pv)
{
    /* 等待连接成功再初始化 MQTT / 启动摄像头 */
    while (1) {
        wifi_async_state_t st = wifi_async_get_state();
        if (st == WIFI_ASYNC_CONNECTED) {
            ESP_LOGI(TAG, "==== 网络就绪，开始业务 ====");
            /* TODO: mqtt_app_start(); */
            break;
        } else if (st == WIFI_ASYNC_FAIL) {
            ESP_LOGW(TAG, "连接失败，可在这里报警/重试");
            /* 可选：wifi_async_start("ssid","pass"); */
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t rc = nvs_flash_init();
    if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* 1. 启动异步 Wi-Fi */
    wifi_async_sta("Readme K80", "1902Pan.");

    /* 2. 继续做别的（LED、外设、日志……） */
    ESP_LOGI(TAG, "Wi-Fi 已提交，主线程继续运行");

    /* 3. 创建业务任务，等待网络就绪 */
    xTaskCreate(business_task, "biz", 4096, NULL, 5, NULL);

     const char *cam_name = NULL;
    if (cam_detect_i2c(&cam_name) == ESP_OK) {
        /* 再初始化摄像头 */
        camera_config_t config = {
            .pin_pwdn  = -1,
            .pin_reset = -1,
            .pin_xclk  = 15,
            .pin_sccb_sda = 4,
            .pin_sccb_scl = 5,
            .xclk_freq_hz = 20000000,
            .pixel_format = PIXFORMAT_JPEG,
            .frame_size   = FRAMESIZE_VGA,
            .fb_count     = 2,
            .fb_location  = CAMERA_FB_IN_PSRAM
        };
        esp_err_t err = esp_camera_init(&config);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Camera driver started, now you can take picture");
        }
    } else {
        ESP_LOGW(TAG, "No camera detected, skip camera driver");
    }
}
