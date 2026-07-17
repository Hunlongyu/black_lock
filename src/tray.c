// tray.c —— 托盘图标与右键菜单实现
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#include <wchar.h>
#include "tray.h"
#include "app.h"

static NOTIFYICONDATAW g_nid = {0};

// 悬浮提示 (三行): 程序名 / 描述 + 快捷键 / 版本号
static void buildTip(const wchar_t *hotkey)
{
    swprintf(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"BlackLock\n无界面黑屏锁屏工具  (%s)\nv" BL_VERSION_W,
             (hotkey && *hotkey) ? hotkey : L"Alt+L");
}

BOOL bl_tray_add(HWND hwnd, HICON icon, const wchar_t *hotkey)
{
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hwnd;
    g_nid.uID              = BL_TRAY_UID;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAY;
    g_nid.hIcon            = icon;
    buildTip(hotkey);
    return Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void bl_tray_update_tip(const wchar_t *hotkey)
{
    buildTip(hotkey);
    g_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// Explorer 重启后任务栏会重建, 原图标已失效, 需要重新 NIM_ADD。
// g_nid 里已保存 hWnd / uID / 图标 / 提示, 直接复用即可。
BOOL bl_tray_readd(void)
{
    if (!g_nid.hWnd)
        return FALSE;
    Shell_NotifyIconW(NIM_DELETE, &g_nid); // 清理可能的残留, 失败可忽略
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    return Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void bl_tray_notify(const wchar_t *title, const wchar_t *msg)
{
    if (!g_nid.hWnd)
        return;
    NOTIFYICONDATAW n = g_nid;
    n.uFlags          = NIF_INFO;
    n.dwInfoFlags     = NIIF_WARNING;
    wcsncpy_s(n.szInfoTitle, ARRAYSIZE(n.szInfoTitle), title, _TRUNCATE);
    wcsncpy_s(n.szInfo, ARRAYSIZE(n.szInfo), msg, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &n);
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
