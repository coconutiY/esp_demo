#include "temperature_dht11.h"
#include "mqtt_client_app.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "dht11";

// 等待 GPIO 变为指定电平，超时返回 -1，否则返回等待耗时 us
static int wait_level(int gpio, int level, int timeout_us)
{
    int elapsed = 0;
    while (gpio_get_level(gpio) != level) {
        if (elapsed >= timeout_us) return -1;
        esp_rom_delay_us(1);
        elapsed++;
    }
    return elapsed;
}

esp_err_t dht11_read(dht11_data_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;

    uint8_t data[5] = {0};
    esp_err_t ret = ESP_OK;

    // 发送起始信号：拉低 ≥18ms（此段不需要禁中断）
    gpio_set_direction(DHT11_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT11_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    // 进入临界区：后续时序在 us 级，禁止中断抢占
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    taskENTER_CRITICAL(&mux);

    gpio_set_direction(DHT11_GPIO, GPIO_MODE_INPUT);
    esp_rom_delay_us(10);

    // 等待 DHT11 响应：先拉低 ~80us，再拉高 ~80us
    if (wait_level(DHT11_GPIO, 0, 200) < 0) {
        taskEXIT_CRITICAL(&mux);
        ESP_LOGE(TAG, "No response (low)");
        return ESP_ERR_TIMEOUT;
    }
    if (wait_level(DHT11_GPIO, 1, 200) < 0) {
        taskEXIT_CRITICAL(&mux);
        ESP_LOGE(TAG, "No response (high)");
        return ESP_ERR_TIMEOUT;
    }
    if (wait_level(DHT11_GPIO, 0, 200) < 0) {
        taskEXIT_CRITICAL(&mux);
        ESP_LOGE(TAG, "No response (end high)");
        return ESP_ERR_TIMEOUT;
    }

    // 读取 40 位数据
    for (int i = 0; i < 40; i++) {
        // 每位以 ~50us 低电平开始
        if (wait_level(DHT11_GPIO, 1, 150) < 0) {
            taskEXIT_CRITICAL(&mux);
            ESP_LOGE(TAG, "Bit %d low timeout", i);
            return ESP_ERR_TIMEOUT;
        }
        // 高电平持续时间：~26us = 0，~70us = 1
        int high_us = wait_level(DHT11_GPIO, 0, 150);
        if (high_us < 0) {
            taskEXIT_CRITICAL(&mux);
            ESP_LOGE(TAG, "Bit %d high timeout", i);
            return ESP_ERR_TIMEOUT;
        }
        data[i / 8] <<= 1;
        if (high_us > 40) {
            data[i / 8] |= 1;
        }
    }

    taskEXIT_CRITICAL(&mux);

    // 校验
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        ESP_LOGE(TAG, "Checksum error: got 0x%02x expected 0x%02x", data[4], checksum);
        return ESP_ERR_INVALID_CRC;
    }

    out->humidity    = data[0] + data[1] * 0.1f;
    out->temperature = data[2] + data[3] * 0.1f;

    ESP_LOGI(TAG, "Temp=%.1f°C  Humi=%.1f%%", out->temperature, out->humidity);
    return ret;
}

static void dht11_task(void *arg)
{
    uint32_t interval_s = (uint32_t)(uintptr_t)arg;
    dht11_data_t data;

    // 配置 GPIO 初始为输入上拉
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << DHT11_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    // 传感器上电稳定等待
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (true) {
        esp_err_t ret = dht11_read(&data);
        if (ret == ESP_OK) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "{\"type\":\"sensor\",\"event\":\"dht11\",\"data\":"
                     "{\"temperature\":%.1f,\"humidity\":%.1f}}",
                     data.temperature, data.humidity);
            mqtt_app_publish_status(msg);
        } else {
            char msg[96];
            snprintf(msg, sizeof(msg),
                     "{\"type\":\"sensor\",\"event\":\"dht11_error\",\"data\":"
                     "{\"error\":\"%s\"}}",
                     esp_err_to_name(ret));
            mqtt_app_publish_status(msg);
        }
        vTaskDelay(pdMS_TO_TICKS(interval_s * 1000));
    }
}

void dht11_start(uint32_t interval_s)
{
    ESP_LOGI(TAG, "DHT11 start on GPIO%d, interval=%lus", DHT11_GPIO, (unsigned long)interval_s);
    xTaskCreate(dht11_task, "dht11", 4096, (void *)(uintptr_t)interval_s, 5, NULL);
}
