#pragma once
#include "esp_err.h"

#define DHT11_GPIO 2

typedef struct {
    float temperature;  // 摄氏度
    float humidity;     // 百分比
} dht11_data_t;

// 初始化，并启动后台采集任务（每 interval_s 秒采集一次并上报 MQTT）
void dht11_start(uint32_t interval_s);

// 单次读取（阻塞，约 20ms）
esp_err_t dht11_read(dht11_data_t *out);
