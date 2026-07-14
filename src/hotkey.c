// hotkey.c —— 快捷键字符串解析与注册
// clang-format off
#include <windows.h>
// clang-format on
#include <wchar.h>
#include <stdlib.h>
#include "hotkey.h"
#include "app.h"

// 解析单个令牌 (已转大写) 为修饰键位; 非修饰键返回 FALSE。
static BOOL parseModifier(const wchar_t *tok, UINT *mods)
{
    if (wcscmp(tok, L"ALT") == 0)
    {
        *mods |= MOD_ALT;
        return TRUE;
    }
    if (wcscmp(tok, L"CTRL") == 0 || wcscmp(tok, L"CONTROL") == 0)
    {
        *mods |= MOD_CONTROL;
        return TRUE;
    }
    if (wcscmp(tok, L"SHIFT") == 0)
    {
        *mods |= MOD_SHIFT;
        return TRUE;
    }
    if (wcscmp(tok, L"WIN") == 0 || wcscmp(tok, L"WINDOWS") == 0)
    {
        *mods |= MOD_WIN;
        return TRUE;
    }
    return FALSE;
}

// 解析主键令牌为虚拟键码; 失败返回 0。
static UINT parseKey(const wchar_t *tok)
{
    size_t len = wcslen(tok);
    if (len == 1)
    {
        wchar_t c = tok[0];
        if (c >= L'A' && c <= L'Z')
            return (UINT)c;
        if (c >= L'0' && c <= L'9')
            return (UINT)c;
    }
    // 功能键 F1..F24
    if ((tok[0] == L'F') && (len == 2 || len == 3))
    {
        int n = _wtoi(tok + 1);
        if (n >= 1 && n <= 24)
            return (UINT)(VK_F1 + (n - 1));
    }
    return 0;
}

BOOL bl_hotkey_parse(const wchar_t *s, UINT *mods, UINT *vk)
{
    if (!s || !*s)
        return FALSE;

    wchar_t buf[BL_HOTKEY_MAX];
    lstrcpynW(buf, s, BL_HOTKEY_MAX); // 截断安全拷贝 (Win32, 跨编译器)
    CharUpperW(buf);                  // 就地转大写

    UINT m   = 0;
    UINT key = 0;

    // 按 '+' 手工分词 (避免 wcstok/_s 在不同 CRT 上的差异)
    wchar_t *p = buf;
    while (*p)
    {
        wchar_t tok[BL_HOTKEY_MAX];
        int tl = 0;
        while (*p && *p != L'+')
        {
            if (tl < BL_HOTKEY_MAX - 1)
                tok[tl++] = *p;
            p++;
        }
        if (*p == L'+')
            p++;
        tok[tl] = 0;

        // 去掉令牌两端空白
        wchar_t *t = tok;
        while (*t == L' ' || *t == L'\t')
            t++;
        int el = (int)wcslen(t);
        while (el > 0 && (t[el - 1] == L' ' || t[el - 1] == L'\t'))
            t[--el] = 0;

        if (*t)
        {
            if (!parseModifier(t, &m))
            {
                UINT k = parseKey(t);
                if (k == 0)
                    return FALSE; // 无法识别的主键
                key = k;          // 最后一个非修饰键作为主键
            }
        }
    }

    if (key == 0)
        return FALSE;
    *mods = m;
    *vk   = key;
    return TRUE;
}

BOOL bl_hotkey_register(HWND hwnd, const wchar_t *s)
{
    UINT mods = 0, vk = 0;
    if (!bl_hotkey_parse(s, &mods, &vk))
        return FALSE;
    // MOD_NOREPEAT 防长按连触
    return RegisterHotKey(hwnd, BL_HOTKEY_ID, mods | MOD_NOREPEAT, vk);
}

void bl_hotkey_unregister(HWND hwnd)
{
    UnregisterHotKey(hwnd, BL_HOTKEY_ID);
}
