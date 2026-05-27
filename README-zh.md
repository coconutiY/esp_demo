| 支持的目标 | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ---------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

# Hello World 示例

启动一个 FreeRTOS 任务来打印 “Hello World”。

（有关更多示例信息，请参见上一级 `examples` 目录中的 `README.md` 文件。）

## 如何使用该示例

请根据安装的 Espressif 开发板芯片选择相应的说明：

- [ESP32 入门指南](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html)
- [ESP32-S2 入门指南](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html)


## 示例文件夹内容

项目 **hello_world** 包含一个 C 语言源文件 [hello_world_main.c](main/hello_world_main.c)。该文件位于 `main` 文件夹中。

ESP-IDF 项目使用 CMake 构建。项目构建配置包含在 `CMakeLists.txt` 文件中，这些文件提供描述项目源文件和目标（可执行文件、库或两者）的指令。

下面是项目文件夹中的文件说明：

```
├── CMakeLists.txt
├── pytest_hello_world.py      用于自动化测试的 Python 脚本
├── main
│   ├── CMakeLists.txt
│   └── hello_world_main.c
└── README.md                  你当前正在阅读的文件
```

有关 ESP-IDF 项目结构和内容的更多信息，请参阅 ESP-IDF 编程指南中的 [构建系统](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) 一节。

## 故障排除

* 程序上传失败

    * 硬件连接不正确：运行 `idf.py -p PORT monitor`，并重新启动开发板查看是否有输出日志。
    * 下载波特率过高：在 `menuconfig` 菜单中降低波特率，然后重试。

## 技术支持和反馈

请使用以下反馈渠道：

* 有技术问题，请访问 [esp32.com](https://esp32.com/) 论坛
* 有功能请求或错误报告，请创建 [GitHub issue](https://github.com/espressif/esp-idf/issues)

我们会尽快回复你。
