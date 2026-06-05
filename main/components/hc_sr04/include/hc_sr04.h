#pragma once
#include <stdint.h>

#define HC_SR04_TRIG_GPIO 38
#define HC_SR04_ECHO_GPIO 39

// 启动超声波测距（定时周期采样，结果通过 MQTT 上报）
void hc_sr04_start(uint32_t interval_s);

// 单次阻塞读取（返回 distance_cm，失败返回 < 0）
float hc_sr04_read(void);
