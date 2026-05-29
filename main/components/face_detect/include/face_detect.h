#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    int x, y, w, h;
    float score;
} face_box_t;

// 输入：JPEG 数据；输出：带框 JPEG + 人脸列表
// out_jpeg/out_jpeg_len 由函数内部 malloc，调用方负责 free
esp_err_t face_detect_jpeg(const uint8_t *jpeg_in, size_t jpeg_len,
                           face_box_t *boxes, int *box_count, int max_boxes,
                           uint8_t **out_jpeg, size_t *out_jpeg_len);
