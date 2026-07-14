// keyhook.c —— 低级键盘钩子 + 密码缓冲 + 解锁判定
// 移植并模块化自参考项目 black_screen 的 kbProc / checkPassword, 并扩展:
//   - require_password = false 时: 回车 或 再次按解锁快捷键 即解锁 (无需密码)
//   - require_password = true  时: 需输入正确密码 + 回车
// clang-format off
#include <windows.h>
// clang-format on
#include <string.h>
#include <wchar.h>
#include "keyhook.h"
#include "app.h"
#include "util.h"

static HHOOK g_hook     = NULL;
static HWND g_notify    = NULL;
static wchar_t g_pw[BL_PW_MAX];
static int g_pwLen      = 0;
static BYTE g_target[32];      // 目标密码 SHA-256
static BOOL g_hasTarget = FALSE;

static BOOL g_requirePw = FALSE; // 是否需要密码解锁
static UINT g_hkMods    = 0;     // 解锁快捷键修饰键 (MOD_*)
static UINT g_hkVk      = 0;     // 解锁快捷键主键 (虚拟键码)
static BOOL g_hkArmed   = FALSE;  // 快捷键"已就绪"(避免触发锁定的那次按键的自动重复立即解锁)

void bl_keyhook_set_password(const wchar_t *plain)
{
    char u8[BL_PW_MAX * 3];
    int n = bl_utf16_to_utf8(plain, (int)wcslen(plain), u8, sizeof(u8));
    if (n < 0)
        n = 0;
    g_hasTarget = bl_sha256((BYTE *)u8, (ULONG)n, g_target);
    bl_secure_zero(u8, sizeof(u8));
}

void bl_keyhook_set_require_password(BOOL require)
{
    g_requirePw = require;
}

void bl_keyhook_set_hotkey(UINT mods, UINT vk)
{
    g_hkMods = mods;
    g_hkVk   = vk;
}

void bl_keyhook_clear(void)
{
    bl_secure_zero(g_pw, sizeof(g_pw));
    g_pwLen = 0;
}

// 校验当前输入缓冲是否与目标密码一致
static BOOL checkPassword(void)
{
    if (!g_hasTarget)
        return FALSE;
    char buf[BL_PW_MAX * 3];
    int n = bl_utf16_to_utf8(g_pw, g_pwLen, buf, sizeof(buf));
    if (n < 0)
        n = 0;
    BYTE dig[32];
    BOOL ok = FALSE;
    if (bl_sha256((BYTE *)buf, (ULONG)n, dig))
        ok = (memcmp(dig, g_target, 32) == 0);
    bl_secure_zero(buf, sizeof(buf));
    return ok;
}

BOOL bl_keyhook_should_unlock(void)
{
    if (!g_requirePw)
        return TRUE; // 未启用密码 -> 直接解锁
    return checkPassword();
}

// 锁定期间吞掉一切按键, 把可打印字符攒进密码缓冲。不依赖窗口焦点。
static LRESULT CALLBACK kbProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code != HC_ACTION)
        return CallNextHookEx(g_hook, code, wParam, lParam);

    KBDLLHOOKSTRUCT *k = (KBDLLHOOKSTRUCT *)lParam;
    BOOL down          = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    BOOL up            = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
    DWORD vk           = k->vkCode;

    if (up)
    {
        // 解锁快捷键主键抬起 -> 就绪 (下一次按下才允许"再次快捷键解锁")
        if (g_hkVk && vk == g_hkVk)
            g_hkArmed = TRUE;
        return 1; // keyup 也吞
    }

    if (down)
    {
        // 1) 拦截系统快捷键
        if (vk == VK_LWIN || vk == VK_RWIN)
            return 1;
        BOOL alt   = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        BOOL ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        BOOL win   = ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0;
        if (alt && (vk == VK_TAB || vk == VK_ESCAPE || vk == VK_F4))
            return 1;
        if (ctrl && vk == VK_ESCAPE)
            return 1;

        // 2) 未启用密码时: 再次按下解锁快捷键即可解锁 (需已就绪, 避免自动重复误触)
        if (!g_requirePw && g_hkVk && vk == g_hkVk && g_hkArmed)
        {
            UINT cur = (alt ? MOD_ALT : 0) | (ctrl ? MOD_CONTROL : 0) | (shift ? MOD_SHIFT : 0) |
                       (win ? MOD_WIN : 0);
            if (cur == g_hkMods)
            {
                if (g_notify)
                    PostMessageW(g_notify, WM_APP_ENTER, 0, 0);
                return 1;
            }
        }

        // 3) 功能键
        if (vk == VK_RETURN)
        {
            if (g_notify)
                PostMessageW(g_notify, WM_APP_ENTER, 0, 0);
            return 1;
        }
        if (vk == VK_BACK)
        {
            if (g_pwLen > 0)
                g_pw[--g_pwLen] = 0;
            return 1;
        }
        if (vk == VK_ESCAPE)
        {
            bl_keyhook_clear();
            return 1;
        }

        // 4) 其它键 -> 翻译成字符, 存入密码缓冲 (仅密码模式下有意义)
        BYTE ks[256];
        GetKeyboardState(ks);
        ks[VK_SHIFT]   = shift ? 0x80 : 0;
        ks[VK_CAPITAL] = (GetKeyState(VK_CAPITAL) & 1) ? 1 : 0;
        WCHAR ch[4];
        int r = ToUnicode((UINT)vk, k->scanCode, ks, ch, 4, 0);
        if (r == 1 && ch[0] >= 0x20)
        {
            if (g_pwLen < BL_PW_MAX - 1)
                g_pw[g_pwLen++] = ch[0];
        }
        return 1;
    }

    return 1;
}

BOOL bl_keyhook_install(HWND notifyHwnd)
{
    if (g_hook)
        return TRUE;
    g_notify  = notifyHwnd;
    g_hkArmed = FALSE; // 每次锁定重新就绪, 需先松开快捷键
    bl_keyhook_clear();
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, kbProc, GetModuleHandleW(NULL), 0);
    return (g_hook != NULL);
}

void bl_keyhook_uninstall(void)
{
    if (g_hook)
    {
        UnhookWindowsHookEx(g_hook);
        g_hook = NULL;
    }
    g_notify = NULL;
}
