// hotkey.h —— 全局快捷键解析与注册 (见开发文档 §6.2)
#ifndef BLACKLOCK_HOTKEY_H
#define BLACKLOCK_HOTKEY_H

// clang-format off
#include <windows.h>
// clang-format on

// 解析 "Alt+L" / "Ctrl+Alt+L" 等为 RegisterHotKey 的 fsModifiers 与 vk。
// 成功返回 TRUE。修饰键: Alt/Ctrl/Shift/Win; 主键: 字母数字或 F1..F24。
BOOL bl_hotkey_parse(const wchar_t *s, UINT *mods, UINT *vk);

// 按配置字符串注册全局热键 (id=BL_HOTKEY_ID)。成功返回 TRUE。
BOOL bl_hotkey_register(HWND hwnd, const wchar_t *s);

// 注销热键。
void bl_hotkey_unregister(HWND hwnd);

#endif // BLACKLOCK_HOTKEY_H
