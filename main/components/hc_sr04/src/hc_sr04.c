#include "hc_sr04.h"
#include "mqtt_client_app.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "hc_sr04";

// 发送 Trig 脉冲 >= 10us
static void trig_pulse(void)
{
    gpio_set_level(HC_SR04_TRIG_GPIO, 1);
    esp_rom_delay_us(12);
    gpio_set_level(HC_SR04_TRIG_GPIO, 0);
}

// 等待 Echo 变为高电平，返回等待微秒数；超时返回 -1
static int wait_echo_high(int timeout_us)
{
    int elapsed = 0;
    while (gpio_get_level(HC_SR04_ECHO_GPIO) == 0) {
        if (elapsed >= timeout_us) return -1;
        esp_rom_delay_us(1);
        elapsed++;
    }
    return elapsed;
}

// 等待 Echo 变为低电平，返回高电平持续微秒数；超时返回 -1
static int wait_echo_low(int timeout_us)
{
    int elapsed = 0;
    while (gpio_get_level(HC_SR04_ECHO_GPIO) == 1) {
        if (elapsed >= timeout_us) return -1;
        esp_rom_delay_us(1);
        elapsed++;
    }
    return elapsed;
}

float hc_sr04_read(void)
{
    // Trig 拉低，确保干净
    gpio_set_direction(HC_SR04_TRIG_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(HC_SR04_TRIG_GPIO, 0);

    // Echo 配置为输入
    gpio_set_direction(HC_SR04_ECHO_GPIO, GPIO_MODE_INPUT);

    // 发送 12us 触发脉冲
    trig_pulse();

    // 等待 Echo 上升沿（超时 60ms = HC-SR04 最远距离 ~10m 对应时长）
    int t1 = wait_echo_high(60000);
    if (t1 < 0) {
        ESP_LOGW(TAG, "Echo timeout (no object)");
        return -1.0f;
    }

    // 等待 Echo 下降沿
    int echo_us = wait_echo_low(60000);
    if (echo_us < 0) {
        ESP_LOGW(TAG, "Echo pulse too long");
        return -1.0f;
    }

    // 距离 = 时间(us) / 2 * 声速(34.3 cm/ms) / 1000
    // echo_us (μs) => /2 * 0.0343 cm/μs => echo_us * 0.01715
    float distance = echo_us * 0.01715f;
    if (distance < 2.0f || distance > 400.0f) {
        ESP_LOGW(TAG, "Distance out of range: %.1f cm", distance);
        return -1.0f;
    }

    return distance;
}

static void hc_sr04_task(void *arg)
{
    uint32_t interval_s = (uint32_t)(uintptr_t)arg;

    // Trig 输出
    gpio_config_t trig_cfg = {
        .pin_bit_mask = (1ULL << HC_SR04_TRIG_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&trig_cfg);

    // Echo 输入
    gpio_config_t echo_cfg = {
        .pin_bit_mask = (1ULL << HC_SR04_ECHO_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&echo_cfg);

    // 上电稳定等待
    vTaskDelay(pdMS_TO_TICKS(100));

    while (true) {
        float dist = hc_sr04_read();
        if (dist >= 0.0f) {
            char msg[96];
            snprintf(msg, sizeof(msg),
                     "{\"type\":\"sensor\",\"event\":\"hc_sr04\",\"data\":"
                     "{\"distance\":%.1f}}",
                     dist);
            mqtt_app_publish_status(msg);
            ESP_LOGI(TAG, "Distance=%.1f cm", dist);
        } else {
            char msg[80];
            snprintf(msg, sizeof(msg),
                     "{\"type\":\"sensor\",\"event\":\"hc_sr04_error\",\"data\":"
                     "{\"error\":\"out_of_range\"}}");
            mqtt_app_publish_status(msg);
        }
        vTaskDelay(pdMS_TO_TICKS(interval_s * 1000));
    }
}

void hc_sr04_start(uint32_t interval_s)
{
    ESP_LOGI(TAG, "HC-SR04 start on Trig=GPIO%d, Echo=GPIO%d, interval=%lus",
             HC_SR04_TRIG_GPIO, HC_SR04_ECHO_GPIO, (unsigned long)interval_s);
    xTaskCreate(hc_sr04_task, "hc_sr04", 4096, (void *)(uintptr_t)interval_s, 5, NULL);
}
