// config.h —— 配置加载/保存/打开 (见开发文档 §8)
#ifndef BLACKLOCK_CONFIG_H
#define BLACKLOCK_CONFIG_H

// clang-format off
#include <windows.h>
// clang-format on
#include "app.h"

typedef struct
{
    wchar_t hotkey[BL_HOTKEY_MAX];   // 触发快捷键, 如 L"Alt+L"
    BOOL autostart;                  // 是否开机自启
    wchar_t password[BL_PW_MAX];     // 明文密码
    wchar_t path[MAX_PATH];          // 实际生效的配置文件绝对路径
} BlConfig;

// 定位 -> (缺失则生成默认) -> 读取, 填充 cfg。成功返回 TRUE。
// 查找顺序: exe 同目录 config.ini 优先, 否则 %APPDATA%\BlackLock\config.ini。
BOOL bl_config_load(BlConfig *cfg);

// 仅回写 autostart 值到 cfg->path (保留其余内容与注释)。成功返回 TRUE。
BOOL bl_config_save_autostart(const BlConfig *cfg);

// 用系统默认程序打开当前生效的配置文件 (cfg->path)。
BOOL bl_config_open_in_editor(const BlConfig *cfg);

#endif // BLACKLOCK_CONFIG_H
