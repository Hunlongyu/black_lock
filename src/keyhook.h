// keyhook.h —— 低级键盘钩子与密码校验 (见开发文档 §6.4/§6.5)
#ifndef BLACKLOCK_KEYHOOK_H
#define BLACKLOCK_KEYHOOK_H

// clang-format off
#include <windows.h>
// clang-format on

// 设置目标密码 (明文)。内部计算并保存 SHA-256, 不长期保留明文。
void bl_keyhook_set_password(const wchar_t *plain);

// 设置是否需要密码解锁 (false: 回车/再次快捷键即解锁; true: 需输入密码)。
void bl_keyhook_set_require_password(BOOL require);

// 设置解锁快捷键 (用于"再次按快捷键解锁"; mods 为 MOD_*, vk 为虚拟键码)。
void bl_keyhook_set_hotkey(UINT mods, UINT vk);

// 安装低级键盘钩子。锁定期间吞掉所有按键并收集密码;
// 回车 (或未启用密码时再次按解锁快捷键) 向 notifyHwnd 投递 WM_APP_ENTER。成功返回 TRUE。
BOOL bl_keyhook_install(HWND notifyHwnd);

// 卸载键盘钩子。成功 (或本就未安装) 返回 TRUE;失败时保留句柄以便重试。
BOOL bl_keyhook_uninstall(void);

// 是否应解锁: 未启用密码 -> TRUE; 启用密码 -> 校验当前输入缓冲是否与目标一致。
BOOL bl_keyhook_should_unlock(void);

// 清空输入缓冲 (安全清零)。
void bl_keyhook_clear(void);

#endif // BLACKLOCK_KEYHOOK_H
