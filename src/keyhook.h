// keyhook.h —— 低级键盘钩子与密码校验 (见开发文档 §6.4/§6.5)
#ifndef BLACKLOCK_KEYHOOK_H
#define BLACKLOCK_KEYHOOK_H

// clang-format off
#include <windows.h>
// clang-format on

// 设置目标密码 (明文)。内部计算并保存 SHA-256, 不长期保留明文。
void bl_keyhook_set_password(const wchar_t *plain);

// 安装低级键盘钩子。锁定期间吞掉所有按键并收集密码;
// 按回车时向 notifyHwnd 投递 WM_APP_ENTER。成功返回 TRUE。
BOOL bl_keyhook_install(HWND notifyHwnd);

// 卸载键盘钩子。
void bl_keyhook_uninstall(void);

// 校验当前输入缓冲是否与目标密码一致。
BOOL bl_keyhook_check_password(void);

// 清空输入缓冲 (安全清零)。
void bl_keyhook_clear(void);

#endif // BLACKLOCK_KEYHOOK_H
