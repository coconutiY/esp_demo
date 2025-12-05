#pragma once
#include "esp_err.h"

/**
 * @brief   启动 Wi-Fi STA 模式并阻塞直到拿到 IP 或超时
 *
 * @param[in]  ssid        目标 AP 的 SSID（必须以 '\0' 结尾）
 * @param[in]  pass        目标 AP 的密码（NULL 表示开放网络）
 * @param[in]  timeout_sec 最大等待秒数；<=0 则永久等待
 *
 * @return
 *   @retval ESP_OK   成功获取 IP，可正常通信
 *   @retval ESP_FAIL 连接失败或超时
 *
 * @note
 *   1. 内部会自动重连 5 次，超出后直接返回 ESP_FAIL<br>
 *   2. 成功返回前会把事件组置位，调用者无需再处理
 *
 * @warning
 *   该函数会阻塞调用线程，请确保不在中断里使用
 *
 * @see wifi_disconnect()  esp_restart()
 */
esp_err_t wifi_provision_sta(const char *ssid, const char *pass, int timeout_sec);