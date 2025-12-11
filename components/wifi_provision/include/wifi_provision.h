#pragma once
#include "esp_err.h"

/**
 * @brief 异步wifi连接状态枚举
 */
typedef enum {
    WIFI_ASYNC_IDLE,
    WIFI_ASYNC_CONNECTING,
    WIFI_ASYNC_CONNECTED,
    WIFI_ASYNC_FAIL
} wifi_async_state_t;

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


/**
 * @brief   启动 Wi-Fi STA 模式并返回异步状态
 *
 * @param[in]  ssid        目标 AP 的 SSID（必须以 '\0' 结尾）
 * @param[in]  pass        目标 AP 的密码（NULL 表示开放网络）
 *
 * @return
 *   @retval ESP_OK   启动成功
 *   @retval ESP_FAIL 启动失败
 *
 * @note
 *   1. 启动成功后，调用者需要调用 wifi_async_get_state() 获取状态并做后续处理<br>
 *   2. 启动成功后，会自动重连 5 次，超出后直接返回 ESP_FAIL<br>
 *   3. 启动成功后，会设置事件组，调用者无需再处理
 *
 * @warning
 *   该函数会阻塞调用线程，请确保不在中断里使用
 *
 * @see wifi_disconnect()  esp_restart()
 */
esp_err_t wifi_async_sta(const char *ssid, const char *pass);



/**
 * @brief   获取 Wi-Fi 异步状态
 *
 * @return
 *   @retval WIFI_ASYNC_IDLE       尚未启动
 *   @retval WIFI_ASYNC_CONNECTING 正在连接
 *   @retval WIFI_ASYNC_CONNECTED  已连接
 *   @retval WIFI_ASYNC_FAIL       连接失败
 */
wifi_async_state_t wifi_async_get_state(void);
