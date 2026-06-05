#pragma once
#include <stdbool.h>
#include <stdint.h>

#define IR_TX_GPIO 3   // 红外发射管
#define IR_RX_GPIO 14   // 红外一体化接收头

// 初始化红外收发（RMT TX/RX），接收默认关闭
void ir_remote_start(void);

// 开/关红外接收监听（含 MQTT 状态上报）
void ir_remote_rx_enable(bool enable);

// 发射一帧标准 NEC（8 位地址 + 8 位命令，内部自动补反码）
void ir_remote_send_nec(uint8_t addr, uint8_t cmd);

// 解析 "ir_*" 文本命令：ir_rx_on / ir_rx_off / ir_send <addr> <cmd>
void ir_remote_handle_command(const char *cmd, int len);
