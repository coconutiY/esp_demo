#include "sound_detect.h"
#include "mqtt_client_app.h"
#include "rgb_led.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdbool.h>

static const char *TAG = "sound_detect";
static QueueHandle_t s_evt_queue = NULL;
static volatile bool s_enabled = false;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    if (!s_enabled) return;
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;
    xQueueSendFromISR(s_evt_queue, &gpio_num, NULL);
}

static void sound_task(void *arg)
{
    uint32_t gpio_num;
    while (true) {
        if (!xQueueReceive(s_evt_queue, &gpio_num, portMAX_DELAY)) continue;
        if (!s_enabled) {
            xQueueReset(s_evt_queue);
            continue;
        }

        int level = gpio_get_level(gpio_num);
        ESP_LOGI(TAG, "Sound GPIO level=%d", level);

        if (level == 1) {
            rgb_led_set_rgb(255, 255, 255);
            mqtt_app_publish_status(
                "{\"type\":\"sensor\",\"event\":\"sound\","
                "\"data\":{\"level\":1,\"action\":\"on all\"}}"
            );
        } else {
            rgb_led_off();
            mqtt_app_publish_status(
                "{\"type\":\"sensor\",\"event\":\"sound\","
                "\"data\":{\"level\":0,\"action\":\"off all\"}}"
            );
        }

        // 防抖
        vTaskDelay(pdMS_TO_TICKS(200));
        xQueueReset(s_evt_queue);
    }
}

void sound_detect_enable(bool enable)
{
    s_enabled = enable;
    ESP_LOGI(TAG, "Sound detect %s", enable ? "ENABLED" : "DISABLED");

    char msg[96];
    snprintf(msg, sizeof(msg),
             "{\"type\":\"sensor\",\"event\":\"sound_ctrl\","
             "\"data\":{\"enabled\":%s}}",
             enable ? "true" : "false");
    mqtt_app_publish_status(msg);

    if (!enable) {
        xQueueReset(s_evt_queue);
        rgb_led_off();
    }
}

void sound_detect_start(void)
{
    s_evt_queue = xQueueCreate(4, sizeof(uint32_t));

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << SOUND_DETECT_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,  // 上升沿和下降沿都触发
    };
    gpio_config(&cfg);

    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(ret));
        return;
    }
    gpio_isr_handler_add(SOUND_DETECT_GPIO, gpio_isr_handler,
                         (void *)(uintptr_t)SOUND_DETECT_GPIO);

    xTaskCreate(sound_task, "sound_detect", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Sound detect initialized (default OFF), GPIO%d", SOUND_DETECT_GPIO);
}
