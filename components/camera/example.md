```c
#include "cam_detect.h"
#include "esp_camera.h"
#include "esp_log.h"

void app_main(void)
{
    const char *cam_name = NULL;
    if (cam_detect_i2c(&cam_name) == ESP_OK) {
        /* 再初始化摄像头 */
        camera_config_t config = {
            .pin_pwdn  = -1,
            .pin_reset = -1,
            .pin_xclk  = 15,
            .pin_sccb_sda = 4,
            .pin_sccb_scl = 5,
            .xclk_freq_hz = 20000000,
            .pixel_format = PIXFORMAT_JPEG,
            .frame_size   = FRAMESIZE_VGA,
            .fb_count     = 2,
            .fb_location  = CAMERA_FB_IN_PSRAM
        };
        esp_err_t err = esp_camera_init(&config);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Camera driver started, now you can take picture");
        }
    } else {
        ESP_LOGW(TAG, "No camera detected, skip camera driver");
    }
}
```