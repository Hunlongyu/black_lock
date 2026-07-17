// lockwin.h —— 全屏黑窗与锁定态编排 (见开发文档 §6.3/§6.6)
#ifndef BLACKLOCK_LOCKWIN_H
#define BLACKLOCK_LOCKWIN_H

// clang-format off
#include <windows.h>
// clang-format on

// 注册黑窗窗口类并创建 (初始隐藏)。成功返回 TRUE。
BOOL bl_lockwin_init(HINSTANCE hInst);

// 进入锁定态: 覆盖虚拟桌面、装键盘钩子、防睡眠、阻关机、隐藏光标。
// 必要资源 (黑窗/钩子/定时器) 任一失败则完整回滚并返回 FALSE, 不会进入半锁定状态。
BOOL bl_lock_enter(void);

// 退出锁定态: 卸钩子、隐藏黑窗、恢复电源/光标。
void bl_lock_exit(void);

// 当前是否处于锁定态。
BOOL bl_lock_is_active(void);

#endif // BLACKLOCK_LOCKWIN_H
