// tray.h —— 系统托盘图标与右键菜单 (见开发文档 §6.9)
#ifndef BLACKLOCK_TRAY_H
#define BLACKLOCK_TRAY_H

// clang-format off
#include <windows.h>
// clang-format on

// 添加托盘图标 (回调消息 WM_APP_TRAY)。hotkey 用于悬浮提示第二行。成功返回 TRUE。
BOOL bl_tray_add(HWND hwnd, HICON icon, const wchar_t *hotkey);

// 更新托盘悬浮提示 (快捷键变化时刷新)。
void bl_tray_update_tip(const wchar_t *hotkey);

// 重新添加托盘图标 (用于 Explorer 重启后的 TaskbarCreated 恢复)。
BOOL bl_tray_readd(void);

// 弹出气泡通知 (用于失败/降级提示)。
void bl_tray_notify(const wchar_t *title, const wchar_t *msg);

// 移除托盘图标。
void bl_tray_remove(void);

// 弹出右键菜单, 返回选中的 IDM_* (未选返回 0)。
// autostartChecked / requirePwChecked / pauseChecked 控制勾选项的勾选态。
int bl_tray_track_menu(HWND hwnd, BOOL autostartChecked, BOOL requirePwChecked, BOOL pauseChecked);

#endif // BLACKLOCK_TRAY_H
