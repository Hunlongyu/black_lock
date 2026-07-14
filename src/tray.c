// tray.c —— 托盘图标与右键菜单实现
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#include "tray.h"
#include "app.h"

static NOTIFYICONDATAW g_nid = {0};

BOOL bl_tray_add(HWND hwnd, HICON icon)
{
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hwnd;
    g_nid.uID              = BL_TRAY_UID;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAY;
    g_nid.hIcon            = icon;
    wcscpy_s(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"BlackLock — 黑屏锁 (Alt+L)");
    return Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void bl_tray_remove(void)
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

int bl_tray_track_menu(HWND hwnd, BOOL autostartChecked, BOOL requirePwChecked, BOOL pauseChecked)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return 0;

    // 版本号 (可点击 -> 打开 GitHub 发布页)
    AppendMenuW(menu, MF_STRING, IDM_RELEASES, L"BlackLock v" BL_VERSION_W);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(menu, MF_STRING | (autostartChecked ? MF_CHECKED : 0), IDM_AUTOSTART, L"开机自启");
    AppendMenuW(menu, MF_STRING | (requirePwChecked ? MF_CHECKED : 0), IDM_REQUIRE_PW, L"开启密码");
    AppendMenuW(menu, MF_STRING | (pauseChecked ? MF_CHECKED : 0), IDM_PAUSE, L"暂停");
    AppendMenuW(menu, MF_STRING, IDM_CONFIG, L"配置");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"退出");

    // 官方要求: 弹菜单前置前台, 否则菜单不会自动消失
    SetForegroundWindow(hwnd);
    POINT pt;
    GetCursorPos(&pt);
    int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y,
                                  0, hwnd, NULL);
    DestroyMenu(menu);
    return cmd;
}
