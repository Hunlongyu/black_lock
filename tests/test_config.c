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
#include <stdlib.h>
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

// 取测试 exe 同目录的 config.ini 路径 (堆上, 调用者 free)。支持长路径。
static wchar_t *testCfgPath(void)
{
    wchar_t *dir = (wchar_t *)malloc(BL_PATH_MAX * sizeof(wchar_t));
    if (!dir)
        return NULL;
    if (!bl_get_exe_dir(dir, BL_PATH_MAX) ||
        wcscat_s(dir, BL_PATH_MAX, L"config.ini") != 0)
    {
        free(dir);
        return NULL;
    }
    return dir;
}

// 在测试 exe 同目录写 config.ini (bl_config_load 会优先命中它)
static void writeCfg(const char *body)
{
    wchar_t *path = testCfgPath();
    if (!path)
        return;
    wchar_t *apiPath = bl_file_api_path(path);
    free(path);
    if (!apiPath)
        return;
    HANDLE h =
        CreateFileW(apiPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    free(apiPath);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD w = 0;
    WriteFile(h, body, (DWORD)strlen(body), &w, NULL);
    CloseHandle(h);
}

// 统计 config.ini 中某个键出现的次数 (用于迁移幂等性)
static int countKeyOccurrences(const char *key)
{
    wchar_t *path = testCfgPath();
    if (!path)
        return -1;
    wchar_t *apiPath = bl_file_api_path(path);
    free(path);
    if (!apiPath)
        return -1;
    HANDLE h = CreateFileW(apiPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    free(apiPath);
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

static BOOL createTestDirectory(const wchar_t *path)
{
    wchar_t *apiPath = bl_file_api_path(path);
    if (!apiPath)
        return FALSE;
    BOOL ok = CreateDirectoryW(apiPath, NULL);
    if (!ok && GetLastError() == ERROR_ALREADY_EXISTS)
        ok = TRUE;
    free(apiPath);
    return ok;
}

// 在没有 longPathAware 清单的测试 exe 中实际读写 >260 字符路径，验证 \\?\ 前缀生效。
static void testLongPathFileIo(void)
{
    wchar_t *dir      = (wchar_t *)malloc(BL_PATH_MAX * sizeof(wchar_t));
    wchar_t *filePath = (wchar_t *)malloc(BL_PATH_MAX * sizeof(wchar_t));
    BOOL ok           = dir && filePath;
    size_t rootLen    = 0;

    if (ok)
    {
        DWORD n = GetTempPathW(BL_PATH_MAX, dir);
        ok = n > 0 && n < BL_PATH_MAX &&
             swprintf(dir + n, BL_PATH_MAX - n, L"BlackLockPathTest-%lu", GetCurrentProcessId()) >
                 0;
    }
    if (ok)
    {
        rootLen = wcslen(dir);
        ok      = createTestDirectory(dir);
    }
    while (ok && wcslen(dir) < 300)
    {
        ok = wcscat_s(dir, BL_PATH_MAX, L"\\segment-0123456789abcdef") == 0 &&
             createTestDirectory(dir);
    }

    wchar_t *apiFile = NULL;
    if (ok)
    {
        ok = swprintf(filePath, BL_PATH_MAX, L"%s\\probe.txt", dir) > 0;
        if (ok)
            apiFile = bl_file_api_path(filePath);
        ok = apiFile != NULL;
    }
    if (ok)
    {
        HANDLE h =
            CreateFileW(apiFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        ok = h != INVALID_HANDLE_VALUE;
        if (ok)
        {
            static const char data[] = "ok";
            DWORD wrote             = 0;
            ok = WriteFile(h, data, sizeof(data) - 1, &wrote, NULL) && wrote == sizeof(data) - 1;
            CloseHandle(h);
        }
        DeleteFileW(apiFile);
    }
    free(apiFile);

    if (dir && rootLen > 0)
    {
        for (;;)
        {
            wchar_t *apiDir = bl_file_api_path(dir);
            if (apiDir)
            {
                RemoveDirectoryW(apiDir);
                free(apiDir);
            }
            if (wcslen(dir) == rootLen)
                break;
            wchar_t *slash = wcsrchr(dir, L'\\');
            if (!slash || (size_t)(slash - dir) < rootLen)
                break;
            *slash = 0;
        }
    }
    free(filePath);
    free(dir);
    check("path: >260 字符文件实际读写", ok);
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

    // --- 文件 API 扩展长度路径转换 (不依赖系统 LongPathsEnabled 策略) ---
    {
        wchar_t *p = bl_file_api_path(L"C:\\deep\\config.ini");
        check("path: 盘符绝对路径加扩展前缀",
              p && wcscmp(p, L"\\\\?\\C:\\deep\\config.ini") == 0);
        free(p);

        p = bl_file_api_path(L"C:/deep/config.ini");
        check("path: 扩展路径规范化分隔符",
              p && wcscmp(p, L"\\\\?\\C:\\deep\\config.ini") == 0);
        free(p);

        p = bl_file_api_path(L"\\\\server\\share\\config.ini");
        check("path: UNC 路径转扩展前缀",
              p && wcscmp(p, L"\\\\?\\UNC\\server\\share\\config.ini") == 0);
        free(p);

        p = bl_file_api_path(L"\\\\?\\C:\\deep\\config.ini");
        check("path: 已有扩展前缀保持不变",
              p && wcscmp(p, L"\\\\?\\C:\\deep\\config.ini") == 0);
        free(p);

        // 回归 (启动失败 bug): \\?\ 会关闭内核规范化, 故必须先折叠双分隔符再加前缀。
        // 触发场景: APPDATA 带尾部反斜杠 -> 拼出 "...Roaming\\BlackLock" -> 若不折叠则路径非法、启动即失败。
        p = bl_file_api_path(L"C:\\a\\\\b\\config.ini");
        check("path: 折叠重复分隔符 (启动失败回归)",
              p && wcscmp(p, L"\\\\?\\C:\\a\\b\\config.ini") == 0);
        free(p);

        // 回归: \\?\ 下 ".." 不再被解析, 必须先规范化。
        p = bl_file_api_path(L"C:\\a\\b\\..\\c\\config.ini");
        check("path: 解析 .. 段",
              p && wcscmp(p, L"\\\\?\\C:\\a\\c\\config.ini") == 0);
        free(p);

        // 回归: "." 当前段应被移除。
        p = bl_file_api_path(L"C:\\a\\.\\b\\config.ini");
        check("path: 解析 . 段",
              p && wcscmp(p, L"\\\\?\\C:\\a\\b\\config.ini") == 0);
        free(p);

        // 尾部反斜杠 + 双分隔符组合 (最接近真实 APPDATA bug 的形态)。
        p = bl_file_api_path(L"C:\\Users\\x\\Roaming\\\\BlackLock");
        check("path: 尾部/中间双分隔符组合",
              p && wcscmp(p, L"\\\\?\\C:\\Users\\x\\Roaming\\BlackLock") == 0);
        free(p);

        // 空/NULL 输入应安全返回 NULL。
        check("path: NULL 输入返回 NULL", bl_file_api_path(NULL) == NULL);
        check("path: 空串返回 NULL", bl_file_api_path(L"") == NULL);
    }
    testLongPathFileIo();

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
