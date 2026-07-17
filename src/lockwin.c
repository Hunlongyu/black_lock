// lockwin.c —— 全屏黑窗 + 锁定态进入/退出编排
// 移植并模块化自参考项目 black_screen 的 WndProc 与 WinMain 锁定部分。
// clang-format off
#include <windows.h>
// clang-format on
#include <wchar.h>
#include "lockwin.h"
#include "keyhook.h"
#include "app.h"

static HWND g_lockHwnd = NULL;
static BOOL g_active   = FALSE;

// 清除可能"卡住"的修饰键: 向系统补发一次 keyup。
//
// 场景: 用 Alt+L 之类的组合键触发锁定时, Alt/Ctrl 等修饰键正被按住;
//       其 keyup 会被低级键盘钩子吞掉, 导致解锁后系统仍认为该修饰键处于按下状态
//       (表现为菜单乱跳、快捷键失灵)。锁定进入前 / 退出后各补发一次 keyup 清除。
//
// 关键: 必须在键盘钩子"未安装"时调用, 否则合成事件会被自己的钩子再次吞掉。
static void releaseModifiers(void)
{
    const WORD vks[] = {VK_LMENU,    VK_RMENU,  VK_LCONTROL, VK_RCONTROL,
                        VK_LSHIFT,   VK_RSHIFT, VK_LWIN,     VK_RWIN};
    INPUT in[ARRAYSIZE(vks)];
    ZeroMemory(in, sizeof(in));
    for (int i = 0; i < (int)ARRAYSIZE(vks); i++)
    {
        in[i].type       = INPUT_KEYBOARD;
        in[i].ki.wVk     = vks[i];
        in[i].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    SendInput(ARRAYSIZE(vks), in, sizeof(INPUT));
}

// 覆盖整个虚拟桌面 (多屏拼接区域)。成功返回 TRUE。
static BOOL coverVirtualDesktop(void)
{
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vw <= 0 || vh <= 0)
        return FALSE;
    return SetWindowPos(g_lockHwnd, HWND_TOPMOST, vx, vy, vw, vh, SWP_SHOWWINDOW);
}

// 前台窗口是否属于任务管理器。
// 优先按进程映像名 Taskmgr.exe 判定 (版本无关, 兼容 Win11 重写的新版任务管理器);
// 旧版类名 "TaskManagerWindow" 作为快速兜底。
static BOOL foregroundIsTaskMgr(HWND fg)
{
    if (!fg)
        return FALSE;

    // 快速路径: 旧版任务管理器类名
    wchar_t cls[64] = {0};
    if (GetClassNameW(fg, cls, ARRAYSIZE(cls)) > 0 && wcscmp(cls, L"TaskManagerWindow") == 0)
        return TRUE;

    // 通用路径: 进程映像名 == Taskmgr.exe
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid == 0)
        return FALSE;
    // PROCESS_QUERY_LIMITED_INFORMATION 可跨完整性级别查询 (任务管理器通常高完整性)
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
        return FALSE;
    wchar_t path[MAX_PATH];
    DWORD sz = ARRAYSIZE(path);
    BOOL ok  = QueryFullProcessImageNameW(h, 0, path, &sz);
    CloseHandle(h);
    if (!ok)
        return FALSE;
    const wchar_t *base = wcsrchr(path, L'\\');
    base                = base ? base + 1 : path;
    return (_wcsicmp(base, L"Taskmgr.exe") == 0);
}

static LRESULT CALLBACK LockProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_APP_ENTER:
        if (bl_keyhook_should_unlock())
        {
            bl_keyhook_clear();
            bl_lock_exit();
        }
        else
        {
            bl_keyhook_clear();
            // 密码错误 -> 蜂鸣提示。
            // 不用托盘气泡: 锁定时黑窗全屏置顶, 气泡要么被盖住(等于没提示),
            // 要么飘在纯黑屏上破坏观感, 且 Win11 toast 时机不可控。蜂鸣可靠且不破坏黑屏。
            MessageBeep(MB_ICONHAND);
        }
        return 0;

    case WM_SETCURSOR:
        SetCursor(NULL); // 隐藏鼠标指针
        return TRUE;

    case WM_TIMER:
        if (g_active)
        {
            // 逃生口保障: 若任务管理器在前台 (用户按 Ctrl+Alt+Del 打开), 则让到它之下
            // 并放弃置顶, 使其可见、可用鼠标"结束进程"解锁; 否则维持置顶盖住桌面。
            // Ctrl+Alt+Del 取消返回后, 前台不再是任务管理器, 下一拍会重新盖上。
            HWND fg = GetForegroundWindow();
            if (foregroundIsTaskMgr(fg))
                // 沉到最底并放弃置顶, 让任务管理器完全露出, 供鼠标"结束进程"解锁
                SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            else
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;

    case WM_DISPLAYCHANGE:
        if (g_active)
            coverVirtualDesktop(); // 分辨率变化/热插拔 -> 重新覆盖
        return 0;

    case WM_QUERYENDSESSION:
        return g_active ? FALSE : TRUE; // 锁定期间软阻止关机/注销

    case WM_CLOSE:
        return 0; // 忽略关闭请求

    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

BOOL bl_lockwin_init(HINSTANCE hInst)
{
    WNDCLASSW wc     = {0};
    wc.lpfnWndProc   = LockProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = BL_LOCK_CLASS;
    wc.hCursor       = NULL;
    if (!RegisterClassW(&wc))
        return FALSE;

    // 初始隐藏, 尺寸暂 0; 进入锁定时再铺满虚拟桌面
    g_lockHwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, BL_LOCK_CLASS, L"", WS_POPUP, 0,
                                 0, 0, 0, NULL, NULL, hInst, NULL);
    return (g_lockHwnd != NULL);
}

BOOL bl_lock_is_active(void)
{
    return g_active;
}

// 进入锁定态。成功返回 TRUE。
//
// 分阶段初始化 + 失败回滚: 只有当"黑窗已覆盖"与"键盘钩子已装上"两个必要条件都成立时,
// 才提交 g_active。否则会出现最糟的两种局面:
//   - 钩子装上了但黑窗没显示 -> 桌面看着正常, 键盘却被吞光;
//   - 黑窗显示了但钩子没装上 -> 一片黑却收不到按键, 密码打不进去, 只能 Ctrl+Alt+Del。
BOOL bl_lock_enter(void)
{
    if (g_active || !g_lockHwnd)
        return FALSE;

    // 先清掉触发热键残留的 Alt/Ctrl 等修饰键 (此时钩子尚未安装, 合成事件能到达系统)
    releaseModifiers();

    // 1) 必要条件: 黑窗铺满虚拟桌面
    if (!coverVirtualDesktop())
        return FALSE; // 尚未改动任何状态

    // 2) 必要条件: 键盘钩子
    if (!bl_keyhook_install(g_lockHwnd))
    {
        ShowWindow(g_lockHwnd, SW_HIDE); // 回滚已显示的黑窗
        return FALSE;
    }

    // 3) 必要条件: 置顶定时器 (Ctrl+Alt+Del 逃生口依赖它给任务管理器让位)
    if (!SetTimer(g_lockHwnd, BL_TIMER_TOP, 1000, NULL))
    {
        bl_keyhook_uninstall(); // 逆序回滚
        ShowWindow(g_lockHwnd, SW_HIDE);
        return FALSE;
    }

    // 4) 必要资源齐备 -> 正式提交锁定态
    g_active = TRUE;

    // 5) 以下为增强项, 失败不影响"能解锁"这一底线, 故不回滚
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
    SetForegroundWindow(g_lockHwnd);
    while (ShowCursor(FALSE) >= 0)
    {
    }
    ShutdownBlockReasonCreate(g_lockHwnd, L"屏幕已锁定");
    return TRUE;
}

void bl_lock_exit(void)
{
    if (!g_active)
        return;

    // 必须先确认钩子已卸载再退出锁定:
    // 若带着"卸不掉的钩子"隐藏黑窗, 用户会得到一个看似正常、键盘却全废的桌面 —— 比继续锁着更糟。
    // 因此卸载失败时重试一次, 仍失败则保持锁定并蜂鸣告警 (Ctrl+Alt+Del 逃生口仍可用)。
    if (!bl_keyhook_uninstall() && !bl_keyhook_uninstall())
    {
        MessageBeep(MB_ICONHAND);
        return; // 维持锁定态, 不做半吊子退出
    }

    g_active = FALSE;
    KillTimer(g_lockHwnd, BL_TIMER_TOP);

    // 钩子已卸载, 补发修饰键 keyup, 清除锁定期间被吞掉的 Alt/Ctrl 等抬起事件,
    // 避免解锁后修饰键"卡住"。
    releaseModifiers();

    ShutdownBlockReasonDestroy(g_lockHwnd);

    ShowWindow(g_lockHwnd, SW_HIDE);
    while (ShowCursor(TRUE) < 0)
    {
    }
    SetThreadExecutionState(ES_CONTINUOUS); // 恢复默认电源策略
    bl_keyhook_clear();
}
