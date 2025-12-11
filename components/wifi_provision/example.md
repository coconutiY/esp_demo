```c++
// 使用案例 
//1、 同步
void app_main(void)
{
    esp_err_t rc = wifi_provision_sta(WIFI_SSID, WIFI_PASS, 30);
    if (rc != ESP_OK)
    {
        ESP_LOGE(TAG, "provision fail, reboot");
        esp_restart();
    }
}
//2、 异步
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
    wifi_async_start("Readme K80", "1902Pan.");

    /* 2. 继续做别的（LED、外设、日志……） */
    ESP_LOGI(TAG, "Wi-Fi 已提交，主线程继续运行");

    /* 3. 创建业务任务，等待网络就绪 */
    xTaskCreate(business_task, "biz", 4096, NULL, 5, NULL);
}

```