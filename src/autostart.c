// autostart.c —— 注册表开机自启实现
// clang-format off
#include <windows.h>
// clang-format on
#include <wchar.h>
#include <stdio.h>
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
        wchar_t exe[MAX_PATH];
        wchar_t quoted[MAX_PATH + 2];
        if (bl_get_exe_path(exe, MAX_PATH))
        {
            swprintf(quoted, MAX_PATH + 2, L"\"%s\"", exe);
            DWORD bytes = (DWORD)((wcslen(quoted) + 1) * sizeof(wchar_t));
            ok = (RegSetValueExW(hKey, BL_APP_NAME, 0, REG_SZ, (const BYTE *)quoted, bytes) ==
                  ERROR_SUCCESS);
        }
    }
    else
    {
        LONG r = RegDeleteValueW(hKey, BL_APP_NAME);
        ok     = (r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND);
    }
    RegCloseKey(hKey);
    return ok;
}
