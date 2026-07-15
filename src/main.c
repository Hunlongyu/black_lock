// main.c —— 入口、生命周期、消息循环 (见开发文档 §5/§6.1)
//
// 状态机: INIT -> IDLE <-> LOCKED
//   IDLE  : 无可见窗口, 仅托盘图标; 监听全局快捷键 (未暂停时)
//   LOCKED: 全屏黑窗 + 键盘钩子, 输入密码回车解锁
//
// 逃生口: Ctrl+Alt+Del 属内核级序列, 用户态无法拦截; 忘记密码可经任务管理器结束进程。
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#include <wchar.h>
#include "app.h"
#include "config.h"
#include "hotkey.h"
#include "autostart.h"
#include "lockwin.h"
#include "keyhook.h"
#include "tray.h"

static BlConfig g_cfg;
static BOOL g_paused   = FALSE;
static HWND g_mainHwnd = NULL;

// 把当前配置推送给键盘钩子: 密码 / 是否需要密码 / 解锁快捷键。
static void syncKeyhookFromConfig(void)
{
    bl_keyhook_set_password(g_cfg.password);
    bl_keyhook_set_require_password(g_cfg.require_password);
    UINT mods = 0, vk = 0;
    if (bl_hotkey_parse(g_cfg.hotkey, &mods, &vk))
        bl_keyhook_set_hotkey(mods, vk);
    else
        bl_keyhook_set_hotkey(0, 0);
}

// 重新加载配置并即时应用: 密码 / 是否需要密码 / 开机自启 / 快捷键。
// 供启动、以及配置文件热重载调用。
static void applyConfig(HWND hwnd)
{
    // 若配置文件此刻不存在 (编辑器保存的瞬间可能短暂删除重建), 跳过本次,
    // 避免读到半成品或误把默认配置写回覆盖用户改动。等下一次变更通知即可。
    if (g_cfg.path[0] && GetFileAttributesW(g_cfg.path) == INVALID_FILE_ATTRIBUTES)
        return;

    BlConfig tmp;
    if (!bl_config_load(&tmp))
        return;
    g_cfg = tmp;

    // 密码 / 是否需要密码 / 快捷键 即时生效
    syncKeyhookFromConfig();

    // 开机自启与配置保持一致 (幂等; 手改配置文件也能同步注册表)
    bl_autostart_sync(g_cfg.autostart);

    // 快捷键即时生效 (暂停中则保持未注册)
    if (!g_paused)
    {
        bl_hotkey_unregister(hwnd);
        bl_hotkey_register(hwnd, g_cfg.hotkey);
    }
}

static void onMenuCommand(HWND hwnd, int cmd)
{
    switch (cmd)
    {
    case IDM_RELEASES:
        ShellExecuteW(NULL, L"open", BL_RELEASES_URL, NULL, NULL, SW_SHOWNORMAL);
        break;

    case IDM_AUTOSTART:
        g_cfg.autostart = !g_cfg.autostart;
        bl_autostart_sync(g_cfg.autostart);
        bl_config_save_bool(&g_cfg, "autostart", g_cfg.autostart);
        break;

    case IDM_REQUIRE_PW:
        g_cfg.require_password = !g_cfg.require_password;
        bl_keyhook_set_require_password(g_cfg.require_password);
        bl_config_save_bool(&g_cfg, "require_password", g_cfg.require_password);
        break;

    case IDM_PAUSE:
        g_paused = !g_paused;
        if (g_paused)
            bl_hotkey_unregister(hwnd);
        else
            bl_hotkey_register(hwnd, g_cfg.hotkey);
        break;

    case IDM_CONFIG:
        bl_config_open_in_editor(&g_cfg);
        break;

    case IDM_EXIT:
        DestroyWindow(hwnd);
        break;
    }
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_HOTKEY:
        if (wp == BL_HOTKEY_ID && !g_paused && !bl_lock_is_active())
            bl_lock_enter(); // 密码/快捷键已由热重载保持最新
        return 0;

    case WM_APP_TRAY:
        // 右键或菜单键 -> 弹出上下文菜单
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU)
        {
            // 弹菜单前重新读取配置, 确保勾选态与磁盘上的配置文件一致
            // (即使文件监视因故漏掉某次变更, 菜单也总是最新的)
            applyConfig(hwnd);
            int cmd = bl_tray_track_menu(hwnd, g_cfg.autostart, g_cfg.require_password, g_paused);
            if (cmd)
                onMenuCommand(hwnd, cmd);
        }
        return 0;

    case WM_DESTROY:
        bl_tray_remove();
        bl_hotkey_unregister(hwnd);
        if (bl_lock_is_active())
            bl_lock_exit();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// 用 WinMain (而非 wWinMain) 作入口: 避免 MinGW 需要 -municode; 命令行本工具不使用。
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    (void)hPrev;
    (void)lpCmd;
    (void)nShow;

    // --- 单实例 ---
    HANDLE mtx = CreateMutexW(NULL, TRUE, BL_MUTEX_NAME);
    if (mtx && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(mtx);
        return 0;
    }

    // --- DPI 感知 (多屏/缩放下精确覆盖) ---
    typedef HANDLE(WINAPI * SetCtx_t)(HANDLE);
    SetCtx_t setCtx =
        (SetCtx_t)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    if (setCtx)
        setCtx((HANDLE)(LONG_PTR)-4); // PER_MONITOR_AWARE_V2
    else
        SetProcessDPIAware();

    // --- 加载配置 ---
    if (!bl_config_load(&g_cfg))
    {
        MessageBoxW(NULL, L"无法读取或创建配置文件", L"BlackLock", MB_ICONERROR);
        return 1;
    }

    // --- 同步开机自启 ---
    bl_autostart_sync(g_cfg.autostart);

    // --- 初始: 密码 / 是否需要密码 / 解锁快捷键 ---
    syncKeyhookFromConfig();

    // --- 创建隐藏主窗口 (承载热键与托盘回调, 永不显示) ---
    WNDCLASSW wc     = {0};
    wc.lpfnWndProc   = MainProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = BL_MAIN_CLASS;
    if (!RegisterClassW(&wc))
        return 1;

    // 隐藏的工具窗口 (不进任务栏/Alt+Tab): 承载全局热键与托盘回调。
    // 用普通隐藏窗口而非 message-only, 以确保 WM_HOTKEY 可靠投递。
    g_mainHwnd = CreateWindowExW(WS_EX_TOOLWINDOW, BL_MAIN_CLASS, BL_APP_NAME, WS_POPUP, 0, 0, 0, 0,
                                 NULL, NULL, hInst, NULL);
    if (!g_mainHwnd)
        return 1;

    // --- 初始化黑窗 ---
    if (!bl_lockwin_init(hInst))
        return 1;

    // --- 注册全局快捷键 ---
    if (!bl_hotkey_register(g_mainHwnd, g_cfg.hotkey))
    {
        // 热键被占用: 静默继续 (仍可经托盘"配置"改键后重启)。见开发文档 D5。
    }

    // --- 托盘图标 ---
    HICON icon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    if (!icon)
        icon = LoadIconW(NULL, IDI_APPLICATION);
    bl_tray_add(g_mainHwnd, icon);

    // --- 配置热重载: 监视配置文件所在目录, 变更即重载 (密码/快捷键/自启即时生效) ---
    wchar_t watchDir[MAX_PATH];
    wcscpy_s(watchDir, MAX_PATH, g_cfg.path);
    wchar_t *slash = wcsrchr(watchDir, L'\\');
    if (slash)
        slash[1] = 0; // 只保留目录部分
    HANDLE hChange = FindFirstChangeNotificationW(
        watchDir, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);

    // --- 消息循环 ---
    if (hChange != INVALID_HANDLE_VALUE)
    {
        // 同时等待 [配置变更] 与 [窗口消息]
        for (;;)
        {
            DWORD w = MsgWaitForMultipleObjects(1, &hChange, FALSE, INFINITE, QS_ALLINPUT);
            if (w == WAIT_OBJECT_0)
            {
                applyConfig(g_mainHwnd); // 配置目录有改动 -> 重载并即时应用
                FindNextChangeNotification(hChange);
            }
            else if (w == WAIT_OBJECT_0 + 1)
            {
                MSG m;
                BOOL quit = FALSE;
                while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE))
                {
                    if (m.message == WM_QUIT)
                    {
                        quit = TRUE;
                        break;
                    }
                    TranslateMessage(&m);
                    DispatchMessageW(&m);
                }
                if (quit)
                    break;
            }
            else
            {
                break; // 句柄异常 -> 退出
            }
        }
        FindCloseChangeNotification(hChange);
    }
    else
    {
        // 罕见: 无法监视目录 -> 退化为普通消息循环 (配置改动需重启生效)
        MSG m;
        while (GetMessageW(&m, NULL, 0, 0) > 0)
        {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }

    // --- 清理 ---
    if (mtx)
    {
        ReleaseMutex(mtx);
        CloseHandle(mtx);
    }
    return 0;
}
