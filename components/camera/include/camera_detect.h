#pragma once
#include "esp_err.h"

/**
 * @brief   查询摄像头是否存在
 *
 * @param[in]  ssid        目标 AP 的 SSID（必须以 '\0' 结尾）
 * @return
 *   @retval ESP_OK   存在摄像头，可正常通信
 *   @retval ESP_FAIL 不存在摄像头
 *
 * @note
 *
 * @warning
 *
 * @see 
 */
esp_err_t cam_detect_i2c(const char **out_name);