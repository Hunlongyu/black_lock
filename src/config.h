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
    BOOL require_password;           // 解锁是否需要密码 (false: 回车/再次快捷键即解锁)
    wchar_t password[BL_PW_MAX];     // 明文密码 (仅 require_password=true 时生效)
    wchar_t path[MAX_PATH];          // 实际生效的配置文件绝对路径
    BOOL security_downgraded;        // 配置要求密码但密码为空/无效 -> 已强制降级为无需密码
} BlConfig;

// 定位 -> (缺失则生成默认) -> 读取, 填充 cfg。成功返回 TRUE。
// 查找顺序: exe 同目录 config.ini 优先, 否则 %APPDATA%\BlackLock\config.ini。
BOOL bl_config_load(BlConfig *cfg);

// 回写某个布尔键 (true/false) 到 cfg->path, 保留其余内容与注释。成功返回 TRUE。
// key 为 "autostart" / "require_password" 等。
BOOL bl_config_save_bool(const BlConfig *cfg, const char *key, BOOL value);

// 用系统默认程序打开当前生效的配置文件 (cfg->path)。
BOOL bl_config_open_in_editor(const BlConfig *cfg);

#endif // BLACKLOCK_CONFIG_H
