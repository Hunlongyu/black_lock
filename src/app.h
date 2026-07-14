// app.h —— 全局共享定义 (常量、消息、菜单 ID)
#ifndef BLACKLOCK_APP_H
#define BLACKLOCK_APP_H

// clang-format off
#include <windows.h>
// clang-format on

#define BL_APP_NAME L"BlackLock"
#define BL_MUTEX_NAME L"Local\\BlackLock_singleton"

// 版本号: 由 CMake 通过 BL_VERSION 编译宏注入 (来源: project() 版本或 CI 的 git tag)。
// 此处为兜底默认值, 正常构建会被覆盖。
#ifndef BL_VERSION
#define BL_VERSION "0.0.0"
#endif
#define BL_WIDEN2(x) L##x
#define BL_WIDEN(x) BL_WIDEN2(x)
#define BL_VERSION_W BL_WIDEN(BL_VERSION) // 宽字符版本号, 用于菜单显示

// 隐藏的主 (message-only) 窗口类名 与 全屏黑窗类名
#define BL_MAIN_CLASS L"BlackLockMain"
#define BL_LOCK_CLASS L"BlackLockOverlay"

// 自定义窗口消息
#define WM_APP_TRAY (WM_APP + 1)  // 托盘图标回调
#define WM_APP_ENTER (WM_APP + 2) // 锁屏中按下回车 -> 校验密码

// 资源与标识 ID
#define BL_HOTKEY_ID 1  // RegisterHotKey 的标识
#define BL_TRAY_UID 1   // 托盘图标 uID
#define BL_TIMER_TOP 1  // 保持置顶定时器

// 托盘右键菜单命令 ID
#define IDM_RELEASES 1000
#define IDM_AUTOSTART 1001
#define IDM_PAUSE 1002
#define IDM_CONFIG 1003
#define IDM_EXIT 1004

// 点击版本项打开的发布页
#define BL_RELEASES_URL L"https://github.com/Hunlongyu/black_lock/releases"

// 密码 / 配置缓冲上限
#define BL_PW_MAX 256
#define BL_HOTKEY_MAX 64

#endif // BLACKLOCK_APP_H
