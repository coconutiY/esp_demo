#include "face_detect.h"
#include "esp_log.h"

static const char *TAG = "face_detect";

esp_err_t face_detect_jpeg(const uint8_t *jpeg_in, size_t jpeg_len,
                           face_box_t *boxes, int *box_count, int max_boxes,
                           uint8_t **out_jpeg, size_t *out_jpeg_len)
{
    ESP_LOGW(TAG, "Camera disabled, face detect unavailable");
    *box_count = 0;
    *out_jpeg = NULL;
    *out_jpeg_len = 0;
    return ESP_ERR_NOT_SUPPORTED;
}
