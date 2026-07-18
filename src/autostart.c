// autostart.c —— 注册表开机自启实现
// clang-format off
#include <windows.h>
// clang-format on
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include "autostart.h"
#include "app.h"
#include "util.h"

#define RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

BOOL bl_autostart_is_enabled(void)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return FALSE;
    LONG r  = RegQueryValueExW(hKey, BL_APP_NAME, NULL, NULL, NULL, NULL);
    RegCloseKey(hKey);
    return (r == ERROR_SUCCESS);
}

BOOL bl_autostart_sync(BOOL enabled)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return FALSE;

    BOOL ok = FALSE;
    if (enabled)
    {
        // 长路径缓冲较大, 走堆 (+3 给两个引号和结尾 NUL)
        wchar_t *exe    = (wchar_t *)malloc(BL_PATH_MAX * sizeof(wchar_t));
        wchar_t *quoted = (wchar_t *)malloc((BL_PATH_MAX + 3) * sizeof(wchar_t));
        if (exe && quoted && bl_get_exe_path(exe, BL_PATH_MAX))
        {
            // 限制的是 exe 路径本身 (不含外层引号): 登录时系统按 Run 值创建进程,
            // 应用名 (引号内部分) 受 MAX_PATH 约束, 引号不计入该上限。
            // 之前把两个引号也算进 MAX_PATH 预算, 会让 259 字符这类"其实能启动"的路径被误拒。
            size_t exeLen = wcslen(exe);
            if (exeLen < MAX_PATH && swprintf(quoted, BL_PATH_MAX + 3, L"\"%s\"", exe) > 0)
            {
                DWORD bytes = (DWORD)((wcslen(quoted) + 1) * sizeof(wchar_t));
                ok          = (RegSetValueExW(hKey, BL_APP_NAME, 0, REG_SZ, (const BYTE *)quoted,
                                              bytes) == ERROR_SUCCESS);
            }
            else
            {
                // exe 路径过长, 注册也无法启动 -> 清掉可能残留的旧路径, 避免登录后启动错误副本。
                RegDeleteValueW(hKey, BL_APP_NAME);
            }
        }
        free(exe);
        free(quoted);
    }
    else
    {
        LONG r = RegDeleteValueW(hKey, BL_APP_NAME);
        ok     = (r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND);
    }
    RegCloseKey(hKey);
    return ok;
}
