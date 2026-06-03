#include "rgb_led.h"
#include "mqtt_client_app.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

static const char *TAG = "rgb_led";

typedef struct {
    int gpio;
    ledc_channel_t ch;
    bool used;
} rgb_ch_t;

static rgb_ch_t s_channels[3] = {
    {RGB_PIN_R, LEDC_CHANNEL_0, false},
    {RGB_PIN_G, LEDC_CHANNEL_1, false},
    {RGB_PIN_B, LEDC_CHANNEL_2, false},
};

static bool rgb_initialized = false;

void rgb_led_init(void)
{
    // 配置 LEDC 定时器
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,  // 8bit = 0~255
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 1000,              // 1kHz PWM
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    // 配置三个通道
    for (int i = 0; i < 3; i++) {
        ledc_channel_config_t ch = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = s_channels[i].ch,
            .gpio_num   = s_channels[i].gpio,
            .duty       = 0,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER_0,
        };
        ledc_channel_config(&ch);
    }

    rgb_initialized = true;
    ESP_LOGI(TAG, "RGB LED initialized (PWM 1kHz 8bit) R=G%d G=G%d B=G%d",
             RGB_PIN_R, RGB_PIN_G, RGB_PIN_B);
}

void rgb_led_set_level(int gpio_num, uint8_t level)
{
    if (!rgb_initialized) return;

    for (int i = 0; i < 3; i++) {
        if (s_channels[i].gpio == gpio_num) {
            uint32_t duty = (uint32_t)level;
            ledc_set_duty(LEDC_LOW_SPEED_MODE, s_channels[i].ch, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, s_channels[i].ch);
            ESP_LOGI(TAG, "G%d level=%u", gpio_num, level);
            char msg[96];
            if (level == 0) {
                snprintf(msg, sizeof(msg),
                         "{\"type\":\"led\",\"event\":\"off\",\"data\":{\"gpio\":%d}}",
                         gpio_num);
            } else {
                snprintf(msg, sizeof(msg),
                         "{\"type\":\"led\",\"event\":\"level\",\"data\":{\"gpio\":%d,\"level\":%u}}",
                         gpio_num, level);
            }
            mqtt_app_publish_status(msg);
            return;
        }
    }
    ESP_LOGW(TAG, "Invalid GPIO %d", gpio_num);
}

void rgb_led_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    if (!rgb_initialized) return;
    rgb_led_set_level(RGB_PIN_R, r);
    rgb_led_set_level(RGB_PIN_G, g);
    rgb_led_set_level(RGB_PIN_B, b);
    ESP_LOGI(TAG, "RGB=%u %u %u", r, g, b);
    char msg[128];
    snprintf(msg, sizeof(msg),
             "{\"type\":\"led\",\"event\":\"rgb\",\"data\":{\"r\":%u,\"g\":%u,\"b\":%u}}",
             r, g, b);
    mqtt_app_publish_status(msg);
}

void rgb_led_off(void)
{
    rgb_led_set_rgb(0, 0, 0);
}

void rgb_led_handle_command(const char *cmd, int len)
{
    if (!cmd || len <= 0) return;

    char buf[64];
    int copy_len = len < (int)(sizeof(buf) - 1) ? len : (int)(sizeof(buf) - 1);
    memcpy(buf, cmd, copy_len);
    buf[copy_len] = '\0';

    ESP_LOGI(TAG, "Command: %s", buf);

    // "on all" / "off all"
    if (strncmp(buf, "on all", 6) == 0) {
        rgb_led_set_rgb(255, 255, 255);
        return;
    }
    if (strncmp(buf, "off all", 7) == 0) {
        rgb_led_off();
        return;
    }

    // "rgb R G B"  例如 "rgb 255 128 0"
    int r, g, b;
    if (sscanf(buf, "rgb %d %d %d", &r, &g, &b) == 3) {
        rgb_led_set_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
        return;
    }

    // "level Gxx N"  例如 "level G38 128"
    int gpio_num = 0, level = 0;
    if (sscanf(buf, "level G%d %d", &gpio_num, &level) == 2) {
        rgb_led_set_level(gpio_num, (uint8_t)level);
        return;
    }

    // "on Gxx" / "off Gxx"
    int gpio = 0;
    char action[8] = {0};
    if (sscanf(buf, "%7s G%d", action, &gpio) == 2) {
        if (strcmp(action, "on") == 0) {
            rgb_led_set_level(gpio, 255);
        } else if (strcmp(action, "off") == 0) {
            rgb_led_set_level(gpio, 0);
        }
        return;
    }

    ESP_LOGW(TAG, "Unknown command: %s", buf);
}