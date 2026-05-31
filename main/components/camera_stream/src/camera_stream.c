#include "camera_stream.h"
#include "mqtt_client_app.h"
#include "face_detect.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "mbedtls/base64.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "cam_stream";
static httpd_handle_t s_server = NULL;

/* Goouuu Tech ESP32-S3-CAM OV2640 pin definitions */
#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   15
#define CAM_PIN_SIOD    4
#define CAM_PIN_SIOC    5
#define CAM_PIN_D7     16
#define CAM_PIN_D6     17
#define CAM_PIN_D5     18
#define CAM_PIN_D4     12
#define CAM_PIN_D3     10
#define CAM_PIN_D2      8
#define CAM_PIN_D1      9
#define CAM_PIN_D0     11
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK   13

#define STREAM_BOUNDARY "mjpeg-boundary"
#define STREAM_CONTENT_TYPE "multipart/x-mixed-replace;boundary=" STREAM_BOUNDARY
#define STREAM_BOUNDARY_STR "\r\n--" STREAM_BOUNDARY "\r\n"
#define STREAM_PART_FMT     "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n"

/* 首页：嵌入 <img> 标签自动拉取 /stream */
static const char INDEX_HTML[] =
    "<!DOCTYPE html><html><head><title>ESP32-S3 Camera</title>"
    "<style>body{margin:0;background:#000;display:flex;justify-content:center;align-items:center;height:100vh;}"
    "img{max-width:100%;max-height:100vh;}</style></head>"
    "<body><img src=\"/stream\"></body></html>";

static esp_err_t mqtt_config_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    const char page[] =
        "<!DOCTYPE html>"
        "<html><head><title>MQTT Config</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>"
        "body{font-family:Arial,sans-serif;max-width:500px;margin:40px auto;padding:0 20px}"
        "h2{color:#333}label{display:block;margin-top:15px;font-weight:bold}"
        "input{width:100%;padding:10px;margin-top:5px;box-sizing:border-box;border:1px solid #ccc;border-radius:4px}"
        "button{width:100%;padding:12px;margin-top:20px;background:#007bff;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:16px}"
        "button:hover{background:#0056b3}"
        ".status{margin-top:20px;padding:10px;border-radius:4px}"
        ".success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}"
        ".info{background:#d1ecf1;color:#0c5460;border:1px solid #bee5eb}"
        "a{display:inline-block;margin-top:15px;color:#007bff}"
        "</style></head><body>"
        "<h2>MQTT Broker Configuration</h2>"
        "<form method='POST' action='/mqtt'>"
        "<label for='uri'>Broker URI</label>"
        "<input id='uri' name='uri' type='text' placeholder='mqtt://broker.example.com:1883' required>"
        "<button type='submit'>Save &amp; Connect</button>"
        "</form>"
        "<div class='status info'>"
        "Topics: esp32s3/status/device &amp; esp32s3/status/camera &amp; esp32s3/control"
        "</div>"
        "<a href='/'>Back to Camera</a>"
        "</body></html>";
    return httpd_resp_send(req, page, sizeof(page) - 1);
}

static esp_err_t mqtt_config_post_handler(httpd_req_t *req)
{
    char buf[256] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    /* 解析 uri=xxx（支持 URL 编码） */
    const char *val = strstr(buf, "uri=");
    if (!val) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing uri");
        return ESP_FAIL;
    }
    val += 4;

    char uri[128] = {0};
    int i = 0;
    for (const char *p = val; *p && *p != '&' && i < (int)sizeof(uri) - 1; p++) {
        if (*p == '%' && p[1] && p[2]) {
            char h[3] = {p[1], p[2], 0};
            uri[i++] = (char)strtol(h, NULL, 16);
            p += 2;
        } else if (*p == '+') {
            uri[i++] = ' ';
        } else {
            uri[i++] = *p;
        }
    }

    ESP_LOGI(TAG, "MQTT config: %s", uri);

    esp_err_t err = mqtt_app_configure(uri[0] ? uri : NULL);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "mqtt_app_configure failed: %s", esp_err_to_name(err));
    }

    /* 保存后重新显示配置页 */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/mqtt");
    return httpd_resp_send(req, NULL, 0);
}

static const httpd_uri_t uri_mqtt_get = {
    .uri = "/mqtt", .method = HTTP_GET, .handler = mqtt_config_get_handler,
};
static const httpd_uri_t uri_mqtt_post = {
    .uri = "/mqtt", .method = HTTP_POST, .handler = mqtt_config_post_handler,
};

static esp_err_t detect_handler(httpd_req_t *req)
{
    uint8_t *jpeg_in = NULL;
    uint8_t *jpeg_out = NULL;
    esp_err_t ret = ESP_FAIL;

    // 1. 分块读取 POST body
    size_t content_len = req->content_len;
    if (content_len == 0 || content_len > 1024 * 1024) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_FAIL;
    }

    jpeg_in = heap_caps_malloc(content_len, MALLOC_CAP_SPIRAM);
    if (!jpeg_in) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    size_t recv_size = 0;
    while (recv_size < content_len) {
        int r = httpd_req_recv(req, (char *)jpeg_in + recv_size, content_len - recv_size);
        if (r <= 0) {
            ESP_LOGE(TAG, "Failed to receive POST data");
            free(jpeg_in);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }
        recv_size += r;
    }

    // 2. 人脸检测
    face_box_t boxes[10];
    int box_count = 0;
    size_t jpeg_out_len = 0;

    ret = face_detect_jpeg(jpeg_in, content_len, boxes, &box_count, 10, &jpeg_out, &jpeg_out_len);
    free(jpeg_in);
    jpeg_in = NULL;

    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Face detect failed");
        return ret;
    }

    ESP_LOGI(TAG, "detect: jpeg_out=%zu bytes, free SPIRAM=%u, internal=%u",
             jpeg_out_len,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    // 3. 先把 faces JSON 头通过 chunked 发出去，再流式 base64 编码图片，避免一次性大分配
    httpd_resp_set_type(req, "application/json");

    char head[512];
    int hpos = snprintf(head, sizeof(head), "{\"count\":%d,\"faces\":[", box_count);
    for (int i = 0; i < box_count; i++) {
        hpos += snprintf(head + hpos, sizeof(head) - hpos,
                       "%s{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"score\":%.2f}",
                       i > 0 ? "," : "", boxes[i].x, boxes[i].y, boxes[i].w, boxes[i].h, boxes[i].score);
    }
    hpos += snprintf(head + hpos, sizeof(head) - hpos, "],\"image\":\"");
    httpd_resp_send_chunk(req, head, hpos);

    // 分块对 jpeg_out 做 base64，每块 1.2KB 输入 -> 1.6KB 输出，栈占用很小
    const size_t CHUNK_IN = 1200;  // 必须是 3 的倍数，base64 才能分块对齐
    char b64[1600 + 4];
    for (size_t off = 0; off < jpeg_out_len; off += CHUNK_IN) {
        size_t in_len = jpeg_out_len - off;
        if (in_len > CHUNK_IN) in_len = CHUNK_IN;
        size_t olen = 0;
        if (mbedtls_base64_encode((unsigned char *)b64, sizeof(b64), &olen,
                                  jpeg_out + off, in_len) != 0) {
            ESP_LOGE(TAG, "base64 chunk encode failed");
            break;
        }
        httpd_resp_send_chunk(req, b64, olen);
    }

    free(jpeg_out);
    jpeg_out = NULL;

    httpd_resp_send_chunk(req, "\"}", 2);
    httpd_resp_send_chunk(req, NULL, 0);  // 结束 chunked 传输
    return ESP_OK;
}

static const httpd_uri_t uri_detect = {
    .uri = "/detect", .method = HTTP_POST, .handler = detect_handler,
};

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, sizeof(INDEX_HTML) - 1);
}

static esp_err_t stream_handler(httpd_req_t *req)
{
    esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");

    char part_hdr[64];
    camera_fb_t *fb = NULL;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        size_t hlen = snprintf(part_hdr, sizeof(part_hdr), STREAM_PART_FMT, fb->len);

        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY_STR, sizeof(STREAM_BOUNDARY_STR) - 1);
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, part_hdr, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }

        esp_camera_fb_return(fb);
        fb = NULL;

        if (res != ESP_OK) {
            break;
        }
    }

    if (fb) {
        esp_camera_fb_return(fb);
    }
    return res;
}

static const httpd_uri_t uri_index = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = index_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t uri_stream = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = stream_handler,
    .user_ctx = NULL,
};

static void diag_camera_bus(void)
{
    ESP_LOGI(TAG, "--- Camera bus diagnostic ---");

    /* 1) 在 XCLK 引脚输出 10MHz，让摄像头能上电启动内部时序 */
    ledc_timer_config_t t = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_1_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 10000000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "diag: ledc timer init failed: %s", esp_err_to_name(err));
        return;
    }
    ledc_channel_config_t c = {
        .gpio_num   = CAM_PIN_XCLK,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 1,
        .hpoint     = 0,
    };
    err = ledc_channel_config(&c);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "diag: ledc channel init failed: %s", esp_err_to_name(err));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));  /* 摄像头电源 / PLL 稳定 */

    /* 2) 在 SDA/SCL 上扫描 7-bit I2C 地址 */
    i2c_master_bus_config_t bus_cfg = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .i2c_port                     = I2C_NUM_0,
        .sda_io_num                   = CAM_PIN_SIOD,
        .scl_io_num                   = CAM_PIN_SIOC,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    err = i2c_new_master_bus(&bus_cfg, &bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "diag: i2c bus init failed: %s", esp_err_to_name(err));
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        return;
    }

    ESP_LOGI(TAG, "Scanning I2C on SDA=%d SCL=%d (XCLK=10MHz on GPIO%d):",
             CAM_PIN_SIOD, CAM_PIN_SIOC, CAM_PIN_XCLK);
    int found = 0;
    for (uint16_t addr = 1; addr < 127; addr++) {
        if (i2c_master_probe(bus, addr, 30) == ESP_OK) {
            ESP_LOGI(TAG, "  >> device responded at 7-bit addr 0x%02X", addr);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, "  no I2C device on the camera bus");
        ESP_LOGW(TAG, "  => sensor not powered, not wired, or FPC reversed");
    } else {
        ESP_LOGI(TAG, "  %d device(s) found on bus", found);
    }

    /* 清理，让 esp_camera_init 接管 */
    i2c_del_master_bus(bus);
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ESP_LOGI(TAG, "--- End diagnostic ---");
}

static int try_wake_camera(void)
{
    /* 候选 GPIO：未被摄像头/USB-JTAG/strapping/PSRAM 占用，且在 Goouuu Tech S3-CAM
       上可能引出的脚。如果某个脚在你的板子上被其它电路占用，副作用一般是无害的 —
       因为我们只是输出 0/1 几十毫秒。 */
    static const int candidates[] = {1, 2, 14, 21, 38, 39, 40, 41, 42, 47};
    static const uint8_t sensor_addrs[] = {
        0x21,  /* GC0308 */
        0x30,  /* OV2640 */
        0x3C,  /* OV3660 / OV5640 */
        0x36,  /* OV7670 */
        0x42,  /* OV7725 */
        0x60,  /* BF3005 / NT99141 写地址 */
    };

    ESP_LOGI(TAG, "--- GPIO wake-up attempt ---");

    /* 起 XCLK 让摄像头有时钟可用 */
    ledc_timer_config_t t = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_1_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 10000000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&t) != ESP_OK) {
        ESP_LOGE(TAG, "wake: ledc timer failed");
        return -1;
    }
    ledc_channel_config_t c = {
        .gpio_num   = CAM_PIN_XCLK,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 1,
        .hpoint     = 0,
    };
    if (ledc_channel_config(&c) != ESP_OK) {
        ESP_LOGE(TAG, "wake: ledc channel failed");
        return -1;
    }

    i2c_master_bus_config_t bus_cfg = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .i2c_port                     = I2C_NUM_0,
        .sda_io_num                   = CAM_PIN_SIOD,
        .scl_io_num                   = CAM_PIN_SIOC,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    if (i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK) {
        ESP_LOGE(TAG, "wake: i2c bus init failed");
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        return -1;
    }

    int hit_gpio = -1, hit_level = -1, hit_addr = -1;

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]) && hit_gpio < 0; i++) {
        int gpio = candidates[i];
        gpio_config_t gcfg = {
            .pin_bit_mask = 1ULL << gpio,
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        if (gpio_config(&gcfg) != ESP_OK) {
            ESP_LOGW(TAG, "wake: gpio%d config failed, skip", gpio);
            continue;
        }

        for (int level = 0; level <= 1 && hit_gpio < 0; level++) {
            gpio_set_level(gpio, level);
            vTaskDelay(pdMS_TO_TICKS(80));  /* 让 sensor 反应 */

            for (size_t k = 0; k < sizeof(sensor_addrs); k++) {
                if (i2c_master_probe(bus, sensor_addrs[k], 20) == ESP_OK) {
                    hit_gpio  = gpio;
                    hit_level = level;
                    hit_addr  = sensor_addrs[k];
                    break;
                }
            }
            ESP_LOGI(TAG, "  GPIO%d driven %s -> %s",
                     gpio,
                     level ? "HIGH" : "LOW",
                     hit_addr >= 0 ? "SENSOR FOUND" : "no response");
        }

        if (hit_gpio < 0) {
            /* 没用，恢复为输入 */
            gpio_reset_pin(gpio);
        }
    }

    i2c_del_master_bus(bus);
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);

    if (hit_gpio >= 0) {
        ESP_LOGI(TAG, "*** WAKE-UP SUCCESS ***");
        ESP_LOGI(TAG, "*** sensor 0x%02X responded when GPIO%d = %s",
                 hit_addr, hit_gpio, hit_level ? "HIGH" : "LOW");
        ESP_LOGI(TAG, "*** keep this GPIO at the same level for esp_camera_init");
        ESP_LOGI(TAG, "*** suggest: set CAM_PIN_PWDN = %d (or RESET) and adjust polarity",
                 hit_gpio);
        /* 保持该 GPIO 不重置，esp_camera_init 接下来就能扫到 sensor */
    } else {
        ESP_LOGE(TAG, "*** WAKE-UP FAILED — none of the candidate GPIOs help");
        ESP_LOGE(TAG, "*** very likely hardware fault: dead sensor, broken SDA/SCL trace, or dead camera LDO");
    }
    return hit_gpio;
}

esp_err_t camera_stream_init(void)
{
    diag_camera_bus();
    try_wake_camera();

    camera_config_t config = {
        .pin_pwdn       = CAM_PIN_PWDN,
        .pin_reset      = CAM_PIN_RESET,
        .pin_xclk       = CAM_PIN_XCLK,
        .pin_sccb_sda   = CAM_PIN_SIOD,
        .pin_sccb_scl   = CAM_PIN_SIOC,
        .pin_d7         = CAM_PIN_D7,
        .pin_d6         = CAM_PIN_D6,
        .pin_d5         = CAM_PIN_D5,
        .pin_d4         = CAM_PIN_D4,
        .pin_d3         = CAM_PIN_D3,
        .pin_d2         = CAM_PIN_D2,
        .pin_d1         = CAM_PIN_D1,
        .pin_d0         = CAM_PIN_D0,
        .pin_vsync      = CAM_PIN_VSYNC,
        .pin_href       = CAM_PIN_HREF,
        .pin_pclk       = CAM_PIN_PCLK,
        .xclk_freq_hz   = 20000000,
        .ledc_timer     = LEDC_TIMER_0,
        .ledc_channel   = LEDC_CHANNEL_0,
        .pixel_format   = PIXFORMAT_JPEG,
        .frame_size     = FRAMESIZE_QVGA,
        .jpeg_quality   = 45,
        .fb_count       = 1,
        .fb_location    = CAMERA_FB_IN_PSRAM,
        .grab_mode      = CAMERA_GRAB_LATEST,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Camera OV2640 initialized (VGA, JPEG quality %d)", config.jpeg_quality);
    return ESP_OK;
}

esp_err_t http_server_start(void)
{
    if (s_server) return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port       = 80;
    config.stack_size        = 8192;
    config.max_uri_handlers  = 10;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(err));
        return err;
    }
    httpd_register_uri_handler(s_server, &uri_mqtt_get);
    httpd_register_uri_handler(s_server, &uri_mqtt_post);
    httpd_register_uri_handler(s_server, &uri_detect);
    ESP_LOGI(TAG, "HTTP server ready — MQTT config at http://<ip>/mqtt");
    ESP_LOGI(TAG, "Face detect at http://<ip>/detect");
    return ESP_OK;
}

esp_err_t camera_stream_start(void)
{
    esp_err_t err = http_server_start();
    if (err != ESP_OK) return err;

    httpd_register_uri_handler(s_server, &uri_index);
    httpd_register_uri_handler(s_server, &uri_stream);
    ESP_LOGI(TAG, "Stream ready — open http://<device-ip>/ in browser");
    ESP_LOGI(TAG, "MQTT config — open http://<device-ip>/mqtt in browser");
    return ESP_OK;
}

void camera_stream_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    esp_camera_deinit();
}
