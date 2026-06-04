#include "ir_remote.h"
#include "mqtt_client_app.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "ir_remote";

// RMT 分辨率 1MHz => 1 tick = 1us，下面所有时序常量单位均为 us
#define IR_RESOLUTION_HZ    1000000

// NEC 协议时序（us）
#define NEC_LEAD_HIGH       9000
#define NEC_LEAD_LOW        4500
#define NEC_REPEAT_LOW      2250
#define NEC_BIT_HIGH        560
#define NEC_ZERO_LOW        560
#define NEC_ONE_LOW         1690

// 一帧 NEC = 1(引导) + 32(数据) + 1(结束) = 34 个 symbol
#define NEC_FRAME_SYMBOLS   34
// 接收缓冲，留足余量
#define IR_RX_BUF_SYMBOLS   64

static rmt_channel_handle_t s_tx_chan = NULL;
static rmt_channel_handle_t s_rx_chan = NULL;
static rmt_encoder_handle_t s_copy_encoder = NULL;
static QueueHandle_t s_rx_queue = NULL;          // 传递接收到的 symbol 数量
static rmt_symbol_word_t s_rx_symbols[IR_RX_BUF_SYMBOLS];
static volatile bool s_rx_enabled = false;
static volatile bool s_inited = false;

static rmt_receive_config_t s_rx_cfg = {
    .signal_range_min_ns = 1250,        // 滤除 < 1.25us 的毛刺
    .signal_range_max_ns = 12000000,    // 单电平超过 12ms 视为帧结束（> 9ms 引导脉冲）
};

// ±25% 容差判断
static inline bool nec_in_range(uint32_t dur, uint32_t target)
{
    return dur > (target - target / 4) && dur < (target + target / 4);
}

/* ------------------------- 发射 ------------------------- */

void ir_remote_send_nec(uint8_t addr, uint8_t cmd)
{
    if (!s_inited || !s_tx_chan || !s_copy_encoder) {
        ESP_LOGW(TAG, "TX not ready");
        return;
    }

    // 标准 NEC：addr, ~addr, cmd, ~cmd，LSB 先发
    uint32_t data = (uint32_t)addr
                  | ((uint32_t)(uint8_t)(~addr) << 8)
                  | ((uint32_t)cmd << 16)
                  | ((uint32_t)(uint8_t)(~cmd) << 24);

    rmt_symbol_word_t frame[NEC_FRAME_SYMBOLS];

    // 引导码：9ms 载波 + 4.5ms 空闲
    frame[0].level0 = 1; frame[0].duration0 = NEC_LEAD_HIGH;
    frame[0].level1 = 0; frame[0].duration1 = NEC_LEAD_LOW;

    for (int i = 0; i < 32; i++) {
        uint32_t bit = (data >> i) & 0x1;
        frame[1 + i].level0 = 1; frame[1 + i].duration0 = NEC_BIT_HIGH;
        frame[1 + i].level1 = 0;
        frame[1 + i].duration1 = bit ? NEC_ONE_LOW : NEC_ZERO_LOW;
    }

    // 结束位：560us 载波 + 一段空闲
    frame[33].level0 = 1; frame[33].duration0 = NEC_BIT_HIGH;
    frame[33].level1 = 0; frame[33].duration1 = NEC_BIT_HIGH;

    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    esp_err_t ret = rmt_transmit(s_tx_chan, s_copy_encoder, frame, sizeof(frame), &tx_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_transmit failed: %s", esp_err_to_name(ret));
        return;
    }
    rmt_tx_wait_all_done(s_tx_chan, 100);

    ESP_LOGI(TAG, "NEC sent addr=0x%02X cmd=0x%02X", addr, cmd);
    char msg[112];
    snprintf(msg, sizeof(msg),
             "{\"type\":\"ir\",\"event\":\"send\",\"data\":{\"address\":%u,\"command\":%u}}",
             addr, cmd);
    mqtt_app_publish_status(msg);
}

/* ------------------------- 接收 ------------------------- */

// 在 ISR 中被调用：把收到的 symbol 数量投递给解析任务
static bool IRAM_ATTR rx_done_cb(rmt_channel_handle_t chan,
                                 const rmt_rx_done_event_data_t *edata,
                                 void *user_ctx)
{
    BaseType_t high_wakeup = pdFALSE;
    size_t num = edata->num_symbols;
    xQueueSendFromISR(s_rx_queue, &num, &high_wakeup);
    return high_wakeup == pdTRUE;
}

// 解析 NEC，成功返回 true 并输出 8 位地址/命令；is_repeat 标记重复帧
static bool nec_decode(const rmt_symbol_word_t *sym, size_t num,
                       uint8_t *addr, uint8_t *cmd, bool *is_repeat)
{
    *is_repeat = false;

    // 重复帧：9ms + 2.25ms（+ 结束位）
    if (num >= 2 && nec_in_range(sym[0].duration0, NEC_LEAD_HIGH) &&
        nec_in_range(sym[0].duration1, NEC_REPEAT_LOW)) {
        *is_repeat = true;
        return true;
    }

    // 完整帧需要 1(引导) + 32(数据) [+ 1(结束)]
    if (num < NEC_FRAME_SYMBOLS - 1) {
        return false;
    }
    if (!nec_in_range(sym[0].duration0, NEC_LEAD_HIGH) ||
        !nec_in_range(sym[0].duration1, NEC_LEAD_LOW)) {
        return false;
    }

    uint32_t data = 0;
    for (int i = 0; i < 32; i++) {
        const rmt_symbol_word_t *b = &sym[1 + i];
        if (!nec_in_range(b->duration0, NEC_BIT_HIGH)) {
            return false;
        }
        if (nec_in_range(b->duration1, NEC_ONE_LOW)) {
            data |= (1u << i);
        } else if (!nec_in_range(b->duration1, NEC_ZERO_LOW)) {
            return false;
        }
    }

    uint8_t a  = data & 0xFF;
    uint8_t na = (data >> 8) & 0xFF;
    uint8_t c  = (data >> 16) & 0xFF;
    uint8_t nc = (data >> 24) & 0xFF;

    // 标准 NEC 反码校验
    if ((a ^ na) != 0xFF || (c ^ nc) != 0xFF) {
        ESP_LOGW(TAG, "NEC checksum mismatch (raw=0x%08lX), maybe extended NEC",
                 (unsigned long)data);
        return false;
    }

    *addr = a;
    *cmd  = c;
    return true;
}

static void ir_rx_task(void *arg)
{
    size_t num;
    while (true) {
        if (!xQueueReceive(s_rx_queue, &num, portMAX_DELAY)) {
            continue;
        }

        if (s_rx_enabled) {
            uint8_t addr = 0, cmd = 0;
            bool repeat = false;
            if (nec_decode(s_rx_symbols, num, &addr, &cmd, &repeat)) {
                if (repeat) {
                    ESP_LOGI(TAG, "NEC repeat");
                    mqtt_app_publish_status(
                        "{\"type\":\"ir\",\"event\":\"repeat\",\"data\":{}}");
                } else {
                    ESP_LOGI(TAG, "NEC recv addr=0x%02X cmd=0x%02X", addr, cmd);
                    char msg[112];
                    snprintf(msg, sizeof(msg),
                             "{\"type\":\"ir\",\"event\":\"nec\",\"data\":"
                             "{\"address\":%u,\"command\":%u}}",
                             addr, cmd);
                    mqtt_app_publish_status(msg);
                }
            } else {
                ESP_LOGD(TAG, "Unrecognized IR frame, %u symbols", (unsigned)num);
            }
        }

        // RMT RX 是一次性的，处理完后必须重新挂接收
        rmt_receive(s_rx_chan, s_rx_symbols, sizeof(s_rx_symbols), &s_rx_cfg);
    }
}

/* ------------------------- 控制 ------------------------- */

void ir_remote_rx_enable(bool enable)
{
    if (!s_inited) {
        ESP_LOGW(TAG, "not inited");
        return;
    }
    s_rx_enabled = enable;
    ESP_LOGI(TAG, "IR receive %s", enable ? "ENABLED" : "DISABLED");

    char msg[96];
    snprintf(msg, sizeof(msg),
             "{\"type\":\"ir\",\"event\":\"rx_ctrl\",\"data\":{\"enabled\":%s}}",
             enable ? "true" : "false");
    mqtt_app_publish_status(msg);
}

void ir_remote_handle_command(const char *cmd, int len)
{
    if (!cmd || len <= 0) return;

    char buf[48];
    int copy_len = len < (int)(sizeof(buf) - 1) ? len : (int)(sizeof(buf) - 1);
    memcpy(buf, cmd, copy_len);
    buf[copy_len] = '\0';

    ESP_LOGI(TAG, "Command: %s", buf);

    if (strcmp(buf, "ir_rx_on") == 0) {
        ir_remote_rx_enable(true);
        return;
    }
    if (strcmp(buf, "ir_rx_off") == 0) {
        ir_remote_rx_enable(false);
        return;
    }

    int addr = 0, cmd_val = 0;   // "ir_send <addr> <cmd>"，十进制
    if (sscanf(buf, "ir_send %d %d", &addr, &cmd_val) == 2) {
        ir_remote_send_nec((uint8_t)addr, (uint8_t)cmd_val);
        return;
    }

    ESP_LOGW(TAG, "Unknown IR command: %s", buf);
}

/* ------------------------- 初始化 ------------------------- */

void ir_remote_start(void)
{
    if (s_inited) return;

    // --- TX ---
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num       = IR_TX_GPIO,
        .clk_src        = RMT_CLK_SRC_DEFAULT,
        .resolution_hz  = IR_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &s_tx_chan));

    rmt_carrier_config_t carrier = {
        .frequency_hz = 38000,      // 38kHz 载波
        .duty_cycle   = 0.33,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(s_tx_chan, &carrier));

    rmt_copy_encoder_config_t copy_cfg = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_cfg, &s_copy_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_tx_chan));

    // --- RX ---
    rmt_rx_channel_config_t rx_cfg = {
        .gpio_num       = IR_RX_GPIO,
        .clk_src        = RMT_CLK_SRC_DEFAULT,
        .resolution_hz  = IR_RESOLUTION_HZ,
        .mem_block_symbols = IR_RX_BUF_SYMBOLS,
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_cfg, &s_rx_chan));

    s_rx_queue = xQueueCreate(4, sizeof(size_t));
    rmt_rx_event_callbacks_t cbs = { .on_recv_done = rx_done_cb };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(s_rx_chan, &cbs, NULL));
    ESP_ERROR_CHECK(rmt_enable(s_rx_chan));

    xTaskCreate(ir_rx_task, "ir_rx", 4096, NULL, 6, NULL);

    // 挂上第一次接收（解析任务受 s_rx_enabled 控制是否上报）
    ESP_ERROR_CHECK(rmt_receive(s_rx_chan, s_rx_symbols, sizeof(s_rx_symbols), &s_rx_cfg));

    s_inited = true;
    ESP_LOGI(TAG, "IR remote initialized (TX=GPIO%d, RX=GPIO%d, receive default OFF)",
             IR_TX_GPIO, IR_RX_GPIO);
}
