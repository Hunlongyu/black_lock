// tray.h —— 系统托盘图标与右键菜单 (见开发文档 §6.9)
#ifndef BLACKLOCK_TRAY_H
#define BLACKLOCK_TRAY_H

// clang-format off
#include <windows.h>
// clang-format on

// 添加托盘图标 (回调消息 WM_APP_TRAY)。成功返回 TRUE。
BOOL bl_tray_add(HWND hwnd, HICON icon);

// 移除托盘图标。
void bl_tray_remove(void);

// 弹出右键菜单, 返回选中的 IDM_* (未选返回 0)。
// autostartChecked / requirePwChecked / pauseChecked 控制勾选项的勾选态。
int bl_tray_track_menu(HWND hwnd, BOOL autostartChecked, BOOL requirePwChecked, BOOL pauseChecked);

#endif // BLACKLOCK_TRAY_H
