#pragma once
#include <stdbool.h>
#include <stdint.h>

// GPIO 引脚定义
#define RGB_PIN_R  45
#define RGB_PIN_G  20
#define RGB_PIN_B  48

// 初始化 RGB LED（PWM 模式，频率 1kHz）
void rgb_led_init(void);

// 设置单路亮度（0~255）
void rgb_led_set_level(int gpio_num, uint8_t level);

// 同时设置三路（0~255）
void rgb_led_set_rgb(uint8_t r, uint8_t g, uint8_t b);

// 全灭
void rgb_led_off(void);

// 解析 MQTT 指令并执行，格式：
//   "level G38 128"      — GPIO38 设置亮度 128
//   "rgb 255 128 0"      — R=255 G=128 B=0
//   "on G38" / "off G38" — 全亮/全灭
//   "on all" / "off all" — 三路全亮/全灭
void rgb_led_handle_command(const char *cmd, int len);