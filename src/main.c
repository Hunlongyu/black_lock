// main.c —— 入口、生命周期、消息循环 (见开发文档 §5/§6.1)
//
// 状态机: INIT -> IDLE <-> LOCKED
//   IDLE  : 无可见窗口, 仅托盘图标; 监听全局快捷键 (未暂停时)
//   LOCKED: 全屏黑窗 + 键盘钩子, 回车或密码解锁
//
// 逃生口: Ctrl+Alt+Del 属内核级序列, 用户态无法拦截; 忘记密码可经任务管理器结束进程。
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#include <stdlib.h>
#include <wchar.h>
#include "app.h"
#include "config.h"
#include "hotkey.h"
#include "autostart.h"
#include "lockwin.h"
#include "keyhook.h"
#include "tray.h"
#include "util.h"

static BlConfig g_cfg;
static BOOL g_paused          = FALSE;
static HWND g_mainHwnd        = NULL;
static UINT g_taskbarCreated  = 0;     // Explorer 重启后任务栏重建的广播消息
static BOOL g_warnedDowngrade = FALSE; // 密码降级只提示一次, 避免每次重载刷屏

/* ---------- 提示 ---------- */

// 配置要求密码但没有有效密码时, 明确告知用户 (而不是静默按无密码运行)
static void warnIfDowngraded(void)
{
    if (g_cfg.security_downgraded && !g_warnedDowngrade)
    {
        bl_tray_notify(L"BlackLock — 密码未生效",
                       L"配置开启了密码, 但 password 为空或含非法字符。\n"
                       L"密码只能用英文字母和数字。已按\"无需密码\"运行。");
        g_warnedDowngrade = TRUE;
    }
    else if (!g_cfg.security_downgraded)
    {
        g_warnedDowngrade = FALSE; // 恢复正常后, 下次再出问题可再次提示
    }
}

/* ---------- 配置应用 ---------- */

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

// 只重新读取配置到 g_cfg, 不产生任何副作用 (不写注册表/不重注册热键)。
// 用于弹出菜单前刷新勾选态 —— 菜单只是"看", 不该顺带写注册表和重注册热键。
static BOOL reloadConfigOnly(void)
{
    // 若配置文件此刻不存在 (编辑器保存的瞬间可能短暂删除重建), 跳过本次,
    // 避免读到半成品或误把默认配置写回覆盖用户改动。
    if (g_cfg.path[0] && !bl_file_exists(g_cfg.path))
        return FALSE;

    BlConfig tmp;
    if (!bl_config_load(&tmp))
        return FALSE;
    g_cfg = tmp;
    return TRUE;
}

// 重新加载配置并即时应用: 密码 / 是否需要密码 / 开机自启 / 快捷键。
// 供启动与配置文件热重载调用。
static void applyConfig(HWND hwnd)
{
    if (!reloadConfigOnly())
        return;

    syncKeyhookFromConfig();

    // 开机自启与配置保持一致 (幂等; 手改配置文件也能同步注册表)
    if (!bl_autostart_sync(g_cfg.autostart))
        bl_tray_notify(L"BlackLock — 开机自启设置失败",
                       L"程序路径过长或无法写入注册表启动项, 开机自启可能未生效。");

    bl_tray_update_tip(g_cfg.hotkey);
    warnIfDowngraded();

    // 快捷键即时生效 (暂停中则保持未注册)
    if (!g_paused)
    {
        bl_hotkey_unregister(hwnd);
        if (!bl_hotkey_register(hwnd, g_cfg.hotkey))
            bl_tray_notify(L"BlackLock — 快捷键未生效",
                           L"快捷键注册失败 (可能被其它程序占用, 或配置写法有误)。\n"
                           L"请在配置文件中换一个 hotkey。");
    }
}

/* ---------- 锁定 ---------- */

static void doLock(void)
{
    if (g_paused || bl_lock_is_active())
        return;
    // 锁定失败必须让用户知道: 否则按了快捷键却什么都没发生, 无从判断
    if (!bl_lock_enter())
        bl_tray_notify(L"BlackLock — 锁定失败",
                       L"无法进入锁屏 (键盘钩子或窗口创建被拒绝, 可能被安全软件拦截)。\n"
                       L"屏幕未锁定。");
}

/* ---------- 托盘菜单 ---------- */

static void onMenuCommand(HWND hwnd, int cmd)
{
    switch (cmd)
    {
    case IDM_RELEASES:
        ShellExecuteW(NULL, L"open", BL_RELEASES_URL, NULL, NULL, SW_SHOWNORMAL);
        break;

    case IDM_LOCKNOW:
        // 菜单关闭后再锁, 否则菜单的模态循环会与黑窗抢前台
        PostMessageW(hwnd, WM_APP_LOCKNOW, 0, 0);
        break;

    case IDM_AUTOSTART:
    {
        BOOL want = !g_cfg.autostart;
        // 先写系统, 成功后才提交内存与配置文件 —— 避免菜单显示"已开启"而注册表其实没写进去
        if (!bl_autostart_sync(want))
        {
            bl_tray_notify(L"BlackLock — 开机自启设置失败",
                           L"程序路径过长或无法写入注册表启动项, 设置未更改。");
            break;
        }
        g_cfg.autostart = want;
        if (!bl_config_save_bool(&g_cfg, "autostart", want))
            bl_tray_notify(L"BlackLock — 配置保存失败",
                           L"开机自启已生效, 但写回配置文件失败, 重启后可能恢复原值。");
        break;
    }

    case IDM_REQUIRE_PW:
        // 禁止在没有有效密码时开启密码保护:
        // 空密码 -> 空回车即解锁; 含中文/emoji 等 -> 锁屏时根本输不进去。两者都是"假保护"。
        if (!g_cfg.require_password && !g_cfg.password_valid)
        {
            bl_tray_notify(L"BlackLock — 无法开启密码",
                           L"请先在配置文件中设置 password (只能用英文字母和数字), 再开启密码保护。");
            break;
        }
        g_cfg.require_password = !g_cfg.require_password;
        bl_keyhook_set_require_password(g_cfg.require_password);
        if (!bl_config_save_bool(&g_cfg, "require_password", g_cfg.require_password))
            bl_tray_notify(L"BlackLock — 配置保存失败",
                           L"设置已生效, 但写回配置文件失败, 重启后可能恢复原值。");
        break;

    case IDM_PAUSE:
        g_paused = !g_paused;
        if (g_paused)
        {
            bl_hotkey_unregister(hwnd);
        }
        else if (!bl_hotkey_register(hwnd, g_cfg.hotkey))
        {
            bl_tray_notify(L"BlackLock — 快捷键未生效",
                           L"恢复时快捷键注册失败 (可能被其它程序占用)。");
        }
        break;

    case IDM_CONFIG:
        if (!bl_config_open_in_editor(&g_cfg))
            bl_tray_notify(L"BlackLock — 无法打开配置", L"没有已关联的程序可以打开 .ini 文件。");
        break;

    case IDM_EXIT:
        DestroyWindow(hwnd);
        break;
    }
}

/* ---------- 窗口过程 ---------- */

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // Explorer 崩溃/重启后任务栏重建 -> 必须重新添加托盘图标,
    // 否则唯一的管理入口 (暂停/配置/退出) 永久消失, 且单实例互斥体让你也无法重开。
    // 该消息由 RegisterWindowMessageW 动态获得, 不是常量, 只能在 switch 之前判断。
    if (g_taskbarCreated && msg == g_taskbarCreated)
    {
        bl_tray_readd();
        return 0;
    }

    switch (msg)
    {
    case WM_HOTKEY:
        if (wp == BL_HOTKEY_ID)
            doLock(); // 密码/快捷键已由热重载保持最新
        return 0;

    case WM_APP_LOCKNOW: // 托盘"立即锁定"
        doLock();
        return 0;

    case WM_APP_RESTORE: // 第二实例请求: 重建托盘并告知已在运行
        bl_tray_readd();
        bl_tray_notify(L"BlackLock 已在运行", L"程序已在后台运行, 托盘图标已恢复。");
        return 0;

    case WM_APP_TRAY:
        // 右键或菜单键 -> 弹出上下文菜单
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU)
        {
            // 弹菜单前只"读"配置刷新勾选态, 不做副作用 (见 reloadConfigOnly)
            reloadConfigOnly();
            // 开机自启勾选态以注册表实际状态为准, 而不是配置文件里写了什么 ——
            // 两者可能不一致 (如写注册表失败), 菜单应反映系统真实状态。
            BOOL autostartReal = bl_autostart_is_enabled();
            int cmd = bl_tray_track_menu(hwnd, autostartReal, g_cfg.require_password, g_paused);
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

/* ---------- 配置目录监视 (仅关心 config.ini) ---------- */

// 判断本批变更里是否有 config.ini。
// 用 ReadDirectoryChangesW 而非 FindFirstChangeNotification: 后者只说"目录变了",
// 同目录任何无关文件 (含我们自己写的 .tmp) 都会触发一次完整重载 + 注册表同步 + 热键重注册。
static BOOL batchTouchesConfig(const BYTE *buf, DWORD bytes)
{
    if (bytes == 0)
        return TRUE; // 缓冲溢出, 无法得知具体文件 -> 保守重载

    const BYTE *p = buf;
    for (;;)
    {
        const FILE_NOTIFY_INFORMATION *fni = (const FILE_NOTIFY_INFORMATION *)p;
        int cch                            = (int)(fni->FileNameLength / sizeof(WCHAR));
        if (cch == 10 && _wcsnicmp(fni->FileName, L"config.ini", 10) == 0)
            return TRUE;
        if (fni->NextEntryOffset == 0)
            break;
        p += fni->NextEntryOffset;
    }
    return FALSE;
}

/* ---------- 入口 ---------- */

// 用 WinMain (而非 wWinMain) 作入口: 避免 MinGW 需要 -municode; 命令行本工具不使用。
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    (void)hPrev;
    (void)lpCmd;
    (void)nShow;

    // --- 单实例: 已在运行则请求对方恢复托盘图标后退出 ---
    // (比"静默退出"有用: Explorer 异常导致图标丢失时, 再点一次 exe 就能把入口找回来)
    HANDLE mtx = CreateMutexW(NULL, TRUE, BL_MUTEX_NAME);
    if (mtx && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        HWND prev = FindWindowW(BL_MAIN_CLASS, NULL);
        if (prev)
            PostMessageW(prev, WM_APP_RESTORE, 0, 0);
        CloseHandle(mtx);
        return 0;
    }

    // --- DPI 感知 (多屏/缩放下精确覆盖) ---
    // 原型必须与实际一致: SetProcessDpiAwarenessContext 返回 BOOL, 参数为 DPI_AWARENESS_CONTEXT。
    typedef BOOL(WINAPI * SetDpiCtx_t)(HANDLE);
    SetDpiCtx_t setCtx =
        (SetDpiCtx_t)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                            "SetProcessDpiAwarenessContext");
    // API 存在但调用失败时也要回退, 否则会退化成 DPI unaware
    if (!setCtx || !setCtx((HANDLE)(LONG_PTR)-4)) // -4 = PER_MONITOR_AWARE_V2
        SetProcessDPIAware();

    // --- 加载配置 ---
    if (!bl_config_load(&g_cfg))
    {
        MessageBoxW(NULL, L"无法读取或创建配置文件", L"BlackLock", MB_ICONERROR);
        return 1;
    }

    // --- 同步开机自启 (失败稍后经托盘提示; 此时托盘尚未创建) ---
    BOOL autostartOk = bl_autostart_sync(g_cfg.autostart);

    // --- 初始: 密码 / 是否需要密码 / 解锁快捷键 ---
    syncKeyhookFromConfig();

    // --- 注册任务栏重建广播 (Explorer 重启后恢复托盘图标) ---
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

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
    BOOL hotkeyOk = bl_hotkey_register(g_mainHwnd, g_cfg.hotkey);

    // --- 托盘图标 (失败则没有任何可见入口, 属致命错误) ---
    HICON icon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    if (!icon)
        icon = LoadIconW(NULL, IDI_APPLICATION);
    if (!bl_tray_add(g_mainHwnd, icon, g_cfg.hotkey))
    {
        MessageBoxW(NULL, L"无法创建托盘图标, 程序将退出 (否则没有任何可见入口)。", L"BlackLock",
                    MB_ICONERROR);
        return 1;
    }

    // --- 托盘就绪后再补报启动期的失败 ---
    if (!hotkeyOk)
        bl_tray_notify(L"BlackLock — 快捷键未生效",
                       L"快捷键注册失败 (可能被其它程序占用, 或配置写法有误)。\n"
                       L"可用托盘菜单\"立即锁定\", 或在配置文件中换一个 hotkey。");
    if (!autostartOk)
        bl_tray_notify(L"BlackLock — 开机自启设置失败",
                       L"程序路径过长或无法写入注册表启动项, 开机自启可能未生效。");
    warnIfDowngraded();

    // --- 配置热重载: 监视配置所在目录, 仅 config.ini 变更才重载 ---
    HANDLE hDir = INVALID_HANDLE_VALUE;
    wchar_t *watchDir = (wchar_t *)malloc(BL_PATH_MAX * sizeof(wchar_t)); // 长路径, 走堆
    if (watchDir)
    {
        wcscpy_s(watchDir, BL_PATH_MAX, g_cfg.path);
        wchar_t *slash = wcsrchr(watchDir, L'\\');
        if (slash)
            slash[1] = 0; // 只保留目录部分
        wchar_t *apiWatchDir = bl_file_api_path(watchDir);
        if (apiWatchDir)
        {
            hDir = CreateFileW(apiWatchDir, FILE_LIST_DIRECTORY,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                               OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                               NULL);
            free(apiWatchDir);
        }
        free(watchDir); // 目录句柄已开, 路径字符串不再需要
    }
    OVERLAPPED ov = {0};
    DWORD notifyBuf[1024]; // DWORD 数组以保证 FILE_NOTIFY_INFORMATION 要求的对齐
    const DWORD kFilter = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME;
    BOOL watching       = FALSE;

    if (hDir != INVALID_HANDLE_VALUE)
    {
        ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (ov.hEvent)
            watching = ReadDirectoryChangesW(hDir, notifyBuf, sizeof(notifyBuf), FALSE, kFilter,
                                             NULL, &ov, NULL);
    }

    // --- 消息循环 ---
    if (watching)
    {
        for (;;)
        {
            DWORD w = MsgWaitForMultipleObjects(1, &ov.hEvent, FALSE, INFINITE, QS_ALLINPUT);
            if (w == WAIT_OBJECT_0)
            {
                DWORD bytes = 0;
                BOOL got    = GetOverlappedResult(hDir, &ov, &bytes, FALSE);
                ResetEvent(ov.hEvent);

                // 先重新布防再处理: 若等处理完才 re-arm, 处理期间发生的保存会被漏掉
                BOOL rearmed = ReadDirectoryChangesW(hDir, notifyBuf, sizeof(notifyBuf), FALSE,
                                                     kFilter, NULL, &ov, NULL);

                if (got && batchTouchesConfig((const BYTE *)notifyBuf, bytes))
                    applyConfig(g_mainHwnd);

                if (!rearmed)
                    break; // 监视失效, 退出循环 (进程仍会走清理)
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
    }
    else
    {
        // 罕见: 无法监视目录 -> 退化为普通消息循环 (配置改动仍会在弹菜单/锁定前被读到)
        MSG m;
        BOOL r;
        // 必须区分 -1: GetMessage 出错时返回 -1, 与正常退出 (0) 不同, 不能一并当成功
        while ((r = GetMessageW(&m, NULL, 0, 0)) != 0)
        {
            if (r == -1)
                break; // 出错 -> 结束循环, 走正常清理
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }

    // --- 清理 ---
    if (ov.hEvent)
        CloseHandle(ov.hEvent);
    if (hDir != INVALID_HANDLE_VALUE)
        CloseHandle(hDir);
    if (mtx)
    {
        ReleaseMutex(mtx);
        CloseHandle(mtx);
    }
    return 0;
}
