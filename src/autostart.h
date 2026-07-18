// autostart.h —— 开机自启 (HKCU\...\Run) (见开发文档 §6.7)
#ifndef BLACKLOCK_AUTOSTART_H
#define BLACKLOCK_AUTOSTART_H

// clang-format off
#include <windows.h>
// clang-format on

// 查询当前是否已注册开机自启。
BOOL bl_autostart_is_enabled(void);

// 使 Run 键与 enabled 一致: TRUE 写入当前 exe 路径, FALSE 删除。
// 路径超过 Run 项的 MAX_PATH 命令行上限时返回 FALSE。
BOOL bl_autostart_sync(BOOL enabled);

#endif // BLACKLOCK_AUTOSTART_H
