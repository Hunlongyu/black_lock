// test_config.c —— 配置层单元测试 (无 UI, 可在 CI 中运行)
//
// 覆盖历史上真实出过问题的点:
//   - 超长/非法密码曾静默退化为空密码 -> 配 require_password=true 会"空回车即解锁"
//   - 密码字符集限制 (只允许 ASCII 字母数字, 否则锁屏时输不进去)
//   - 老配置缺 require_password 键的自动补齐, 且不得重复追加
// clang-format off
#include <windows.h>
// clang-format on
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "config.h"
#include "util.h"

static int g_fail = 0;

static void check(const char *name, int cond)
{
    printf("%s  %s\n", cond ? "[PASS]" : "[FAIL]", name);
    if (!cond)
        g_fail++;
}

// 在测试 exe 同目录写 config.ini (bl_config_load 会优先命中它)
static void writeCfg(const char *body)
{
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    if (!bl_get_exe_dir(dir, MAX_PATH))
        return;
    swprintf(path, MAX_PATH, L"%sconfig.ini", dir);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD w = 0;
    WriteFile(h, body, (DWORD)strlen(body), &w, NULL);
    CloseHandle(h);
}

// 统计 config.ini 中某个键出现的次数 (用于迁移幂等性)
static int countKeyOccurrences(const char *key)
{
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    if (!bl_get_exe_dir(dir, MAX_PATH))
        return -1;
    swprintf(path, MAX_PATH, L"%sconfig.ini", dir);
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    char buf[8192] = {0};
    DWORD got      = 0;
    ReadFile(h, buf, sizeof(buf) - 1, &got, NULL);
    CloseHandle(h);
    buf[got] = 0;

    int n             = 0;
    const char *p     = buf;
    size_t keylen     = strlen(key);
    while ((p = strstr(p, key)) != NULL)
    {
        // 只统计行首(可含空白)的键, 跳过注释里的同名词
        const char *ls = p;
        while (ls > buf && ls[-1] != '\n')
            ls--;
        const char *q = ls;
        while (*q == ' ' || *q == '\t')
            q++;
        if (q == p && *ls != ';' && *ls != '#')
            n++;
        p += keylen;
    }
    return n;
}

int main(void)
{
    BlConfig c;

    // --- 密码字符集判定 ---
    check("chars: abc123 合法", bl_password_chars_ok(L"abc123"));
    check("chars: 空串非法", !bl_password_chars_ok(L""));
    check("chars: 含空格非法", !bl_password_chars_ok(L"ab cd"));
    check("chars: 含符号非法", !bl_password_chars_ok(L"abc!"));
    check("chars: 中文非法", !bl_password_chars_ok(L"密码"));

    // --- 有效密码 -> 密码保护生效 ---
    writeCfg("[security]\r\nrequire_password = true\r\npassword = abc123\r\n");
    check("load: 合法密码", bl_config_load(&c));
    check("有效密码 -> 保护生效",
          c.require_password && c.password_valid && !c.security_downgraded);

    // --- 空密码 -> 必须降级, 不得冒充已保护 ---
    writeCfg("[security]\r\nrequire_password = true\r\npassword =\r\n");
    bl_config_load(&c);
    check("空密码 -> 强制降级", !c.require_password && !c.password_valid && c.security_downgraded);

    // --- 中文密码 (UTF-8) -> 降级 (锁屏时无法输入) ---
    writeCfg("[security]\r\nrequire_password = true\r\npassword = \xe5\xaf\x86\xe7\xa0\x81\r\n");
    bl_config_load(&c);
    check("中文密码 -> 强制降级", !c.require_password && c.security_downgraded);

    // --- 超长密码 -> 降级 (曾静默变空密码 = 回车即解锁) ---
    {
        char buf[1024];
        strcpy_s(buf, sizeof(buf), "[security]\r\nrequire_password = true\r\npassword = ");
        size_t base = strlen(buf);
        for (int i = 0; i < 300; i++)
            buf[base + i] = 'x';
        buf[base + 300] = 0;
        strcat_s(buf, sizeof(buf), "\r\n");
        writeCfg(buf);
    }
    bl_config_load(&c);
    check("超长密码 -> 强制降级", !c.require_password && c.security_downgraded);

    // --- 默认: 不需要密码 ---
    writeCfg("[security]\r\nrequire_password = false\r\npassword =\r\n");
    bl_config_load(&c);
    check("默认不开启密码", !c.require_password && !c.security_downgraded);

    // --- 快捷键解析 ---
    writeCfg("[general]\r\nhotkey = Ctrl+Alt+K\r\nautostart = true\r\n"
             "[security]\r\nrequire_password = false\r\npassword =\r\n");
    bl_config_load(&c);
    check("hotkey 读取", wcscmp(c.hotkey, L"Ctrl+Alt+K") == 0);
    check("autostart 读取", c.autostart);

    // --- 老配置迁移: 缺 require_password 自动补, 且幂等 ---
    writeCfg("[general]\r\nhotkey = Alt+L\r\n[security]\r\npassword = abc\r\n");
    check("迁移前无 require_password", countKeyOccurrences("require_password") == 0);
    bl_config_load(&c);
    check("迁移后补上 require_password", countKeyOccurrences("require_password") == 1);
    bl_config_load(&c);
    bl_config_load(&c);
    check("重复加载不重复追加", countKeyOccurrences("require_password") == 1);

    // --- 布尔写回 ---
    writeCfg("[general]\r\nautostart = false\r\n[security]\r\nrequire_password = false\r\n"
             "password =\r\n");
    bl_config_load(&c);
    check("save_bool 写回 true", bl_config_save_bool(&c, "autostart", TRUE));
    bl_config_load(&c);
    check("写回后读到 true", c.autostart);

    printf("\n%s (失败 %d 项)\n", g_fail ? "FAILED" : "ALL PASS", g_fail);
    return g_fail ? 1 : 0;
}
