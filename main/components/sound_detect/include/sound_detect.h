#pragma once

#define SOUND_DETECT_GPIO 42

// 初始化声音监测（默认关闭，需通过 sound_on 指令开启）
void sound_detect_start(void);

// 开启/关闭声音监测
void sound_detect_enable(bool enable);
