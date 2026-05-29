#include "face_detect.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "jpeg_decoder.h"
#include "img_converters.h"
#include "esp_camera.h"

static const char *TAG = "face_detect";

// 在 RGB888 缓冲区上画矩形框
static void draw_box(uint8_t *rgb888, int width, int height, face_box_t *box, uint8_t r, uint8_t g, uint8_t b)
{
    int x1 = box->x;
    int y1 = box->y;
    int x2 = box->x + box->w;
    int y2 = box->y + box->h;

    // 边界检查
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= width) x2 = width - 1;
    if (y2 >= height) y2 = height - 1;

    // 画上下边
    for (int x = x1; x <= x2; x++) {
        int idx_top = (y1 * width + x) * 3;
        int idx_bot = (y2 * width + x) * 3;
        rgb888[idx_top] = r; rgb888[idx_top+1] = g; rgb888[idx_top+2] = b;
        rgb888[idx_bot] = r; rgb888[idx_bot+1] = g; rgb888[idx_bot+2] = b;
    }

    // 画左右边
    for (int y = y1; y <= y2; y++) {
        int idx_left = (y * width + x1) * 3;
        int idx_right = (y * width + x2) * 3;
        rgb888[idx_left] = r; rgb888[idx_left+1] = g; rgb888[idx_left+2] = b;
        rgb888[idx_right] = r; rgb888[idx_right+1] = g; rgb888[idx_right+2] = b;
    }
}

esp_err_t face_detect_jpeg(const uint8_t *jpeg_in, size_t jpeg_len,
                           face_box_t *boxes, int *box_count, int max_boxes,
                           uint8_t **out_jpeg, size_t *out_jpeg_len)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t *rgb888 = NULL;
    uint8_t *jpeg_out_buf = NULL;

    // 1. JPEG 解码到 RGB888
    esp_jpeg_image_cfg_t jpeg_cfg = {
        .indata = (uint8_t *)jpeg_in,
        .indata_size = jpeg_len,
        .outbuf = NULL,  // 自动分配
        .outbuf_size = 0,
        .out_format = JPEG_IMAGE_FORMAT_RGB888,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {
            .swap_color_bytes = 0,
        }
    };

    esp_jpeg_image_output_t outimg;
    ret = esp_jpeg_decode(&jpeg_cfg, &outimg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JPEG decode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    rgb888 = jpeg_cfg.outbuf;  // 输出在 cfg.outbuf
    int width = outimg.width;
    int height = outimg.height;
    ESP_LOGI(TAG, "Decoded JPEG: %dx%d", width, height);

    // 2. 人脸检测（TODO: 调用 esp-dl API）
    *box_count = 0;
    // 临时：假设检测到一个人脸用于测试
    if (max_boxes > 0) {
        boxes[0].x = width / 4;
        boxes[0].y = height / 4;
        boxes[0].w = width / 2;
        boxes[0].h = height / 2;
        boxes[0].score = 0.95f;
        *box_count = 1;
    }

    // 3. 画框
    for (int i = 0; i < *box_count; i++) {
        draw_box(rgb888, width, height, &boxes[i], 0, 255, 0);  // 绿色
    }

    // 4. RGB888 编码回 JPEG（使用软件编码器）
    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;
    bool ok = fmt2jpg(rgb888, width * height * 3, width, height, PIXFORMAT_RGB888, 80, &jpg_buf, &jpg_len);
    free(rgb888);
    rgb888 = NULL;

    if (!ok || !jpg_buf) {
        ESP_LOGE(TAG, "JPEG encode failed");
        ret = ESP_FAIL;
        goto cleanup;
    }

    *out_jpeg = jpg_buf;
    *out_jpeg_len = jpg_len;

    ESP_LOGI(TAG, "Face detect complete: %d faces, output JPEG %zu bytes", *box_count, jpg_len);
    ret = ESP_OK;

cleanup:
    if (rgb888) free(rgb888);
    return ret;
}
