# 项目开发规范 Skill

> 本文档定义了 ESP32-S3 项目的开发规范，所有贡献者必须遵循这些规范以确保代码一致性、可维护性和可靠性。

---

## 1. 项目概述

- **目标芯片**: ESP32-S3（Xtensa 双核架构）
- **开发框架**: ESP-IDF v6.0.1
- **项目功能**: 基于 BLE 的 Wi-Fi 配网（Provisioning）方案
- **编程语言**: C（C11 标准）

---

## 2. 项目目录结构规范

```
esp32-s3/
├── CMakeLists.txt                  # 项目顶层构建配置（含 EXTRA_COMPONENT_DIRS 和 project() 声明）
├── sdkconfig                       # 项目配置（由 menuconfig 生成，不手动编辑）
├── sdkconfig.defaults              # 默认配置（仅写入与默认值不同的配置项）
├── main/                           # 主应用模块
│   ├── CMakeLists.txt              # 主模块构建配置
│   ├── Kconfig.projbuild           # 主模块菜单配置
│   ├── hello_world_main.c          # 主程序入口
│   └── components/                 # ★ 自定义组件目录（所有业务组件放在此处）
│       └── <component_name>/       # 单个自定义组件
│           ├── CMakeLists.txt
│           ├── README.md
│           ├── include/
│           │   └── <component_name>.h
│           └── src/
│               └── <component_name>.c
├── components/                     # ESP-IDF 原生组件（勿修改）
├── skill/                          # 开发规范与技能文档
└── README.md                       # 项目说明文档
```

### 2.1 目录职责划分

| 目录 | 职责 | 可否修改 |
|------|------|----------|
| `main/` | 主应用入口与业务逻辑 | ✅ 可修改 |
| `main/components/` | **自定义业务组件** | ✅ 可修改 |
| `components/` | ESP-IDF v6.0.1 原生组件 | ❌ 禁止修改 |
| `skill/` | 开发规范与技能文档 | ✅ 可修改 |

### 2.2 新建组件规范

所有自定义组件**必须**放在 `main/components/` 目录下，禁止放在根目录 `components/` 中。

新建组件的完整步骤：

1. 在 `main/components/` 下创建组件目录，目录名使用 **小写+下划线** 命名（如 `bt_prov`、`wifi_manager`）
2. 组件目录内必须包含以下文件：

```
<component_name>/
├── CMakeLists.txt          # 必须 - 构建配置
├── README.md               # 必须 - 组件说明文档
├── include/
│   └── <component_name>.h  # 必须 - 公共头文件（对外接口）
└── src/
    └── <component_name>.c  # 必须 - 实现文件
```

3. 在 `main/CMakeLists.txt` 的 `PRIV_REQUIRES` 中添加组件依赖

4. 确保顶层 `CMakeLists.txt` 已配置 `EXTRA_COMPONENT_DIRS`：

```cmake
set(EXTRA_COMPONENT_DIRS "main/components")
```

> 此行必须在 `include($ENV{IDF_PATH}/tools/cmake/project.cmake)` 之前，否则构建系统无法发现 `main/components/` 下的自定义组件。

---

## 3. 命名规范

### 3.1 文件命名

| 类型 | 规范 | 示例 |
|------|------|------|
| 组件目录 | 小写+下划线 | `bt_prov/`、`wifi_manager/` |
| 头文件 | 小写+下划线，与组件名一致 | `bt_prov.h` |
| 源文件 | 小写+下划线，与组件名一致 | `bt_prov.c` |
| CMakeLists | 固定名称 | `CMakeLists.txt` |
| README | 固定名称 | `README.md` |

### 3.2 C 语言命名

#### 函数命名
- **公共API**: `组件名_动作()` 格式，如 `bt_prov_init()`、`bt_prov_start()`
- **内部静态函数**: `组件名_模块_动作()` 格式，如 `bt_prov_wifi_init()`、`bt_prov_gap_event_handler()`

#### 类型命名
- **枚举**: `组件名_类型_t`，如 `bt_prov_event_t`
- **结构体**: `组件名_配置_t`，如 `bt_prov_config_t`
- **回调类型**: `组件名_回调描述_t`，如 `bt_prov_event_cb_t`

#### 变量命名
- **静态全局变量**: `s_` 前缀 + 小写下划线，如 `s_initialized`、`s_wifi_connected`
- **宏定义/常量**: 全大写+下划线，如 `GATTS_SERVICE_UUID_PROV`、`MAX_SSID_LEN`
- **局部变量**: 小写下划线，如 `err`、`wifi_config`

#### 日志标签
- 每个模块定义 `static const char *TAG = "模块名";`
- 如 `static const char *TAG = "bt_prov";`

---

## 4. 代码结构规范

### 4.1 头文件模板（`include/<component>.h`）

```c
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 1. 枚举定义
// 2. 回调类型定义
// 3. 配置结构体定义
// 4. 公共API声明（按初始化→启动→停止顺序）

#ifdef __cplusplus
}
#endif
```

**要求**:
- 使用 `#pragma once` 作为头文件保护（而非 `#ifndef` 宏）
- 支持 C++ 兼容（`extern "C"`）
- 头文件仅暴露公共接口，内部实现细节不对外暴露
- 公共头文件仅依赖 `esp_err.h` 等 ESP-IDF 标准头文件，不依赖其他自定义组件的内部头文件

### 4.2 源文件模板（`src/<component>.c`）

```c
#include "<component_name>.h"
#include <string.h>
// ESP-IDF 系统头文件
#include "esp_log.h"
#include "esp_system.h"
// ... 其他依赖

static const char *TAG = "<component_name>";

// 1. 宏定义与常量
// 2. 枚举与类型定义（仅内部使用）
// 3. 静态全局变量
// 4. 内部工具函数（static）
// 5. 事件处理函数（static）
// 6. 子系统初始化函数（static）
// 7. 公共API实现（与头文件声明顺序一致）
```

**要求**:
- 所有内部函数必须使用 `static` 修饰
- 函数实现顺序：被调用者在前，调用者在后（减少前向声明）
- 公共API放在文件末尾，与头文件声明顺序保持一致

### 4.3 组件 CMakeLists.txt 模板

```cmake
idf_component_register(SRCS "src/<component_name>.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES <依赖组件列表>)
```

**要求**:
- `SRCS` 指向 `src/` 目录下的源文件
- `INCLUDE_DIRS` 指向 `include/` 目录
- `PRIV_REQUIRES` 列出所有依赖的组件（私有依赖，不传递给上层）
- 仅依赖真正使用的组件，不要添加多余依赖

### 4.4 组件 README.md 模板

每个组件必须包含 README.md，内容涵盖：

1. **组件功能概述** - 一句话描述组件用途
2. **组件结构** - 文件列表与职责
3. **使用方式** - 集成步骤与代码示例
4. **依赖** - 依赖的 ESP-IDF 组件列表
5. **配置项** - 需要在 sdkconfig 中开启的配置

---

## 5. API 设计规范

### 5.1 公共接口原则

- **最小暴露**: 只暴露必要的初始化、启动、停止接口，内部细节通过回调通知
- **配置结构体**: 使用结构体传参，便于扩展新字段而不破坏 API 兼容性
- **错误返回**: 所有公共API返回 `esp_err_t`，使用 ESP-IDF 标准错误码
- **事件回调**: 异步状态变化通过回调通知，不使用轮询

### 5.2 标准接口模式

每个组件应至少提供以下三个核心接口：

```c
// 初始化组件（仅初始化，不启动服务）
esp_err_t <component>_init(const <component>_config_t *config);

// 启动组件服务
esp_err_t <component>_start(void);

// 停止组件服务
esp_err_t <component>_stop(void);
```

### 5.3 事件回调模式

```c
// 事件枚举：按时间顺序排列
typedef enum {
    <COMPONENT>_EVENT_STARTED,           // 模块已启动
    <COMPONENT>_EVENT_<SUBSYSTEM>_STARTED, // 子系统已启动
    <COMPONENT>_EVENT_<ACTION>_COMPLETE,   // 操作完成
    <COMPONENT>_EVENT_ERROR,              // 错误
} <component>_event_t;

// 回调函数类型
typedef void (*<component>_event_cb_t)(<component>_event_t event, esp_err_t err, const char *info);
```

**要求**:
- `info` 参数传递可读的附加信息（如 IP 地址、SSID 等），可为 NULL
- `err` 参数在正常事件时传 `ESP_OK`，错误事件时传具体错误码
- 回调在中断上下文之外调用，但不应执行耗时操作

---

## 6. 错误处理规范

### 6.1 返回值检查

**所有**返回 `esp_err_t` 的函数调用必须检查返回值：

```c
// ✅ 正确：检查返回值
esp_err_t err = esp_wifi_init(&wifi_init_cfg);
if (err != ESP_OK) {
    ESP_LOGE(TAG, "Wi-Fi init failed: %s", esp_err_to_name(err));
    return err;
}

// ❌ 错误：忽略返回值
esp_wifi_init(&wifi_init_cfg);
```

### 6.2 初始化函数容错

对于可能重复调用的初始化函数，应处理 `ESP_ERR_INVALID_STATE`：

```c
err = esp_netif_init();
if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
}
```

### 6.3 NVS 初始化容错

NVS 初始化应处理分区损坏的情况：

```c
err = nvs_flash_init();
if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
}
```

### 6.4 日志级别使用

| 级别 | 宏 | 使用场景 |
|------|-----|---------|
| INFO | `ESP_LOGI` | 正常流程关键节点（启动、连接、完成） |
| WARNING | `ESP_LOGW` | 非致命异常（可恢复的失败、降级处理） |
| ERROR | `ESP_LOGE` | 致命错误（初始化失败、连接失败） |
| DEBUG | `ESP_LOGD` | 调试信息（开发阶段使用，发布前移除） |

---

## 7. ESP-IDF 特定规范

### 7.1 蓝牙相关

- 使用 BLE 前必须释放经典蓝牙内存：`esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)`
- BT 控制器初始化顺序：`esp_bt_controller_init()` → `esp_bt_controller_enable()` → `esp_bluedroid_init()` → `esp_bluedroid_enable()`
- GATT 服务优先使用 Attribute Table 方式（`esp_ble_gatts_create_attr_tab`），而非手动创建方式
- BLE 广播断开后应自动重启广播

### 7.2 Wi-Fi 相关

- 使用 STA 模式前确保 `esp_netif_init()` 和 `esp_event_loop_create_default()` 已调用
- Wi-Fi 事件和 IP 事件分别注册处理器
- 断开后应实现自动重连机制
- Wi-Fi 密码等敏感信息不应在日志中输出

### 7.3 内存管理

- 优先使用静态分配（静态全局变量），减少运行时动态分配
- 字符串缓冲区使用固定大小数组 + `strncpy` 防止溢出
- 动态分配的资源必须在 `stop` 函数中正确释放

### 7.4 sdkconfig 配置

- 项目必需的配置项写入 `sdkconfig.defaults`，不直接编辑 `sdkconfig`
- 组件特有的配置项通过 `Kconfig.projbuild` 定义
- 必需的蓝牙配置：
  ```
  CONFIG_BT_ENABLED=y
  CONFIG_BTDM_CONTROLLER_MODE_BLE_ONLY=y
  ```

---

## 8. 构建与依赖规范

### 8.1 依赖原则

- 组件依赖声明在 `CMakeLists.txt` 的 `PRIV_REQUIRES` 中
- 仅声明直接使用的依赖，传递依赖由构建系统自动处理
- 自定义组件之间的依赖也需显式声明

### 8.2 依赖方向

```
main (主应用)
 ├── 依赖 → main/components/bt_prov (自定义组件)
 └── 依赖 → spi_flash (IDF组件)

main/components/bt_prov (自定义组件)
 ├── 依赖 → bt (IDF组件)
 ├── 依赖 → nvs_flash (IDF组件)
 ├── 依赖 → esp_netif (IDF组件)
 ├── 依赖 → esp_wifi (IDF组件)
 └── 依赖 → esp_event (IDF组件)
```

**禁止循环依赖**：自定义组件之间不应产生循环依赖。

---

## 9. Git 规范

### 9.1 忽略文件

以下文件/目录不应提交到版本控制：
- `build/` - 构建输出
- `sdkconfig.old` - 旧配置备份
- `managed_components/` - 托管组件（由 idf_component.yml 管理）

### 9.2 提交信息格式

```
<type>(<scope>): <subject>

<body>
```

**type 类型**：
| 类型 | 说明 |
|------|------|
| feat | 新功能 |
| fix | 修复缺陷 |
| refactor | 重构（不改变功能） |
| docs | 文档更新 |
| style | 代码格式调整 |
| chore | 构建/工具变更 |

**scope 范围**：组件名，如 `bt_prov`、`main`、`build`

**示例**：
```
feat(bt_prov): 添加Wi-Fi断开自动重连机制

- 在WIFI_EVENT_STA_DISCONNECTED事件中增加自动重连逻辑
- 添加最大重试次数限制
```

---

## 10. 安全规范

- **密码不明文日志**: Wi-Fi 密码等敏感数据禁止在日志中输出（当前 `bt_prov.c` 仅输出密码长度，符合规范）
- **缓冲区溢出防护**: 所有字符串拷贝使用 `strncpy` 并手动添加终止符 ` `
- **输入长度校验**: BLE 写入数据必须校验长度，不超过预定义的 `MAX_*_LEN`
- **NVS 分区加密**: 生产环境建议启用 NVS 加密保护存储的凭据

---

## 11. 检查清单

每次提交代码前，对照以下清单自查：

- [ ] 新组件放在 `main/components/` 目录下
- [ ] 组件包含 CMakeLists.txt / README.md / include/ / src/ 四部分
- [ ] 公共API返回 `esp_err_t`，所有调用点检查返回值
- [ ] 内部函数使用 `static` 修饰
- [ ] 命名遵循 `组件名_动作()` 规范
- [ ] 静态全局变量使用 `s_` 前缀
- [ ] 日志使用正确的级别（INFO/WARN/ERROR）
- [ ] 敏感信息不在日志中输出
- [ ] 字符串操作使用安全函数（strncpy + 终止符）
- [ ] 未修改 `components/` 下的 ESP-IDF 原生组件
- [ ] 组件依赖在 CMakeLists.txt 中正确声明
- [ ] README.md 已更新
