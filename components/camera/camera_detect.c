#include "camera_detect.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define TAG            "camera_detect"
#define I2C_PORT       I2C_NUM_0
#define I2C_SDA_IO     4          // 按实际修改
#define I2C_SCL_IO     5
#define I2C_FREQ_HZ    100000

/* ---------- 常用摄像头 ID 表 ---------- */
typedef struct {
    uint8_t  addr;      // 7-bit 地址
    uint8_t  reg;       // ID 寄存器
    uint8_t  mask;      // 位掩码
    uint8_t  expect;    // 期望值
    const char *name;
} cam_id_t;

static const cam_id_t cam_table[] = {
    { 0x30, 0x0A, 0xFF, 0x26, "OV2640" },
    { 0x3C, 0x0A, 0xFF, 0x56, "OV5640" },
    { 0x21, 0x0A, 0xFF, 0x76, "OV7670" },
    { 0x21, 0x00, 0xFF, 0x42, "OV7725" },
};
#define CAM_TABLE_SIZE  (sizeof(cam_table)/sizeof(cam_table[0]))

/* ---------- I²C 初始化 ---------- */
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    return i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

/* ---------- 读寄存器 ---------- */
static esp_err_t cam_read_id(uint8_t addr7, uint8_t reg, uint8_t *data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr7 << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr7 << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, 10);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* ---------- 对外接口 ---------- */
esp_err_t cam_detect_i2c(const char **out_name)
{
    *out_name = NULL;
    ESP_ERROR_CHECK(i2c_master_init());

    ESP_LOGI(TAG, "扫描I2C总线...");
    for (size_t i = 0; i < CAM_TABLE_SIZE; i++) {
        uint8_t id = 0;
        if (cam_read_id(cam_table[i].addr, cam_table[i].reg, &id) == ESP_OK) {
            if ((id & cam_table[i].mask) == cam_table[i].expect) {
                ESP_LOGI(TAG, "在地址 0x%02X 发现设备", cam_table[i].addr);
                ESP_LOGI(TAG, "检测到 %s!", cam_table[i].name);
                *out_name = cam_table[i].name;
                return ESP_OK;
            }
        }
    }
    ESP_LOGW(TAG, "未发现已知摄像头");
    return ESP_ERR_NOT_FOUND;
}