# Bluetooth Provisioning Component

该组件提供一个基于 BLE 的 Wi-Fi 配网功能。它在 ESP32 上创建一个 BLE GATT 服务，客户端可以写入 Wi-Fi SSID 和密码，然后自动连接到路由器。

## 组件结构

- `include/bt_prov.h` - 组件公共接口
- `src/bt_prov.c` - BLE 和 Wi-Fi 初始化，GATT 服务实现
- `CMakeLists.txt` - 组件构建脚本

## 使用方式

1. 在 `main/CMakeLists.txt` 中添加组件依赖：

```cmake
idf_component_register(SRCS "hello_world_main.c"
                       PRIV_REQUIRES spi_flash bt_prov
                       INCLUDE_DIRS "")
```

2. 在 `main/hello_world_main.c` 中包含组件头文件并启动服务：

```c
#include "bt_prov.h"
```

3. 通过 BLE 客户端写入 `SSID` 和 `PASSWORD` 特征值进行配网。

## 依赖

- `bt`
- `nvs_flash`
- `esp_netif`
- `esp_wifi`
- `esp_event`

## 建议的 sdkconfig 配置

- `CONFIG_BT_ENABLED=y`
- `CONFIG_BTDM_CONTROLLER_MODE_BLE_ONLY=y`
- `CONFIG_WIFI_STA=y`
