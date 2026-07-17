// config.c —— 配置文件读写 (UTF-8, 支持 ; 与 # 整行注释)
//
// 说明: 采用轻量自写解析器 (零依赖), 以正确处理 UTF-8 与非 ASCII 密码。
// 值行不支持行尾注释 (以免密码含 ; / # 被截断); 注释需独占一行。
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "util.h"

/* ---------- 文件读写辅助 ---------- */

// 读取整个文件到 malloc 缓冲 (追加 NUL)。返回缓冲 (调用者 free), 失败 NULL。
static char *readFileAll(const wchar_t *path, DWORD *outLen)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;

    DWORD size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size > (16u * 1024u * 1024u))
    {
        CloseHandle(h);
        return NULL;
    }
    char *buf = (char *)malloc(size + 1);
    if (!buf)
    {
        CloseHandle(h);
        return NULL;
    }
    DWORD got = 0;
    BOOL ok   = ReadFile(h, buf, size, &got, NULL);
    CloseHandle(h);
    if (!ok)
    {
        free(buf);
        return NULL;
    }
    buf[got] = 0;
    if (outLen)
        *outLen = got;
    return buf;
}

// 原子写: 先整写临时文件 (循环写满 + 落盘), 再整体替换目标。
// 不能直接 CREATE_ALWAYS 覆盖原文件 —— 那样一旦中途失败 (磁盘满/权限/进程被杀),
// 原配置会变成空文件或半截文件, 下次加载就退回"无密码"。
static BOOL writeFileAtomic(const wchar_t *path, const char *data, DWORD len)
{
    wchar_t tmp[MAX_PATH];
    if (swprintf(tmp, MAX_PATH, L"%s.tmp", path) < 0)
        return FALSE;

    HANDLE h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return FALSE;

    BOOL ok          = TRUE;
    const char *p    = data;
    DWORD left       = len;
    while (left > 0)
    {
        DWORD wrote = 0;
        if (!WriteFile(h, p, left, &wrote, NULL) || wrote == 0)
        {
            ok = FALSE;
            break;
        }
        p += wrote;
        left -= wrote;
    }
    if (ok)
        ok = FlushFileBuffers(h); // 确保数据落盘后再替换
    CloseHandle(h);

    if (!ok)
    {
        DeleteFileW(tmp);
        return FALSE;
    }
    // 同卷替换, 失败则保留原文件不动
    if (!MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(tmp);
        return FALSE;
    }
    return TRUE;
}

// 把内嵌的默认配置资源 (BL_DEFAULT_CONFIG, 源自 src/config.default.ini) 写入 path。
static BOOL writeDefaultConfig(const wchar_t *path)
{
    HRSRC hRes = FindResourceW(NULL, L"BL_DEFAULT_CONFIG", RT_RCDATA);
    if (!hRes)
        return FALSE;
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData)
        return FALSE;
    const void *p = LockResource(hData);
    DWORD sz      = SizeofResource(NULL, hRes);
    if (!p || sz == 0)
        return FALSE;
    return writeFileAtomic(path, (const char *)p, sz);
}

/* ---------- 路径定位 ---------- */

// exe 同目录 config.ini。成功返回 TRUE。
// 注意: 取不到 exe 路径 (如路径超长) 时必须返回 FALSE, 绝不能退回相对路径 "config.ini" ——
// 那会读到当前工作目录里不相干的文件 (快捷方式可任意指定工作目录)。
static BOOL pathExeConfig(wchar_t *out, size_t cch)
{
    wchar_t dir[MAX_PATH];
    if (!bl_get_exe_dir(dir, MAX_PATH))
        return FALSE;
    return swprintf(out, cch, L"%sconfig.ini", dir) > 0;
}

// %APPDATA%\BlackLock\config.ini (并确保目录存在)
static BOOL pathAppDataConfig(wchar_t *out, size_t cch, BOOL createDir)
{
    // 用 GetEnvironmentVariableW 并检查长度 (返回值 >= 缓冲即为截断), 而非 _wgetenv 直接拼接
    wchar_t appdata[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return FALSE;

    wchar_t dir[MAX_PATH];
    if (swprintf(dir, MAX_PATH, L"%s\\BlackLock", appdata) < 0)
        return FALSE;
    if (createDir)
        CreateDirectoryW(dir, NULL); // 已存在则忽略
    return swprintf(out, cch, L"%s\\config.ini", dir) > 0;
}

/* ---------- 密码字符集 ---------- */

// 密码只允许 ASCII 字母与数字。
// 原因: 锁定期间靠 WH_KEYBOARD_LL + ToUnicode 逐键翻译收集密码,
//       死键/输入法(IME)/代理项对(emoji)在这条路径上无法可靠输入 ——
//       若允许这些字符, 用户会"设了密码却永远输不进去", 只能 Ctrl+Alt+Del 逃生。
//       因此在加载期就限制字符集并明确告知, 而不是让用户事后被锁在外面。
BOOL bl_password_chars_ok(const wchar_t *pw)
{
    if (!pw || !*pw)
        return FALSE; // 空密码不算有效密码
    for (const wchar_t *p = pw; *p; p++)
    {
        wchar_t c = *p;
        BOOL ok   = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9');
        if (!ok)
            return FALSE;
    }
    return TRUE;
}

/* ---------- 解析辅助 ---------- */

static char *ltrim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

static void rtrim(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = 0;
}

// 把 UTF-8 值写入 UTF-16 目标缓冲。成功返回 TRUE。
// 失败 (如值超出缓冲容量、非法 UTF-8) 时返回 FALSE 并置空, 由调用方判定如何处理 ——
// 关键: 不能把"转换失败"静默当成"空值", 否则超长密码会退化成空密码 (回车即解锁)。
static BOOL utf8ToW(const char *s, wchar_t *out, int cch)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, out, cch);
    if (n <= 0)
    {
        out[0] = 0;
        return FALSE;
    }
    return TRUE;
}

// 解析文本 (原地修改), 填充 cfg 的 hotkey/autostart/password
static void parseIni(char *text, BlConfig *cfg)
{
    char *line = text;
    // 跳过 UTF-8 BOM
    if ((unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB &&
        (unsigned char)line[2] == 0xBF)
        line += 3;

    while (line && *line)
    {
        char *nl = strpbrk(line, "\r\n");
        char *next;
        if (nl)
        {
            *nl  = 0;
            next = nl + 1;
            while (*next == '\r' || *next == '\n')
                next++;
        }
        else
        {
            next = NULL;
        }

        char *p = ltrim(line);
        if (*p && *p != ';' && *p != '#' && *p != '[')
        {
            char *eq = strchr(p, '=');
            if (eq)
            {
                *eq       = 0;
                char *key = p;
                char *val = ltrim(eq + 1);
                rtrim(key);
                rtrim(val);

                if (_stricmp(key, "hotkey") == 0)
                    utf8ToW(val, cfg->hotkey, BL_HOTKEY_MAX);
                else if (_stricmp(key, "password") == 0)
                    utf8ToW(val, cfg->password, BL_PW_MAX);
                else if (_stricmp(key, "autostart") == 0)
                    cfg->autostart =
                        (_stricmp(val, "true") == 0 || _stricmp(val, "1") == 0) ? TRUE : FALSE;
                else if (_stricmp(key, "require_password") == 0)
                    cfg->require_password =
                        (_stricmp(val, "true") == 0 || _stricmp(val, "1") == 0) ? TRUE : FALSE;
            }
        }
        line = next;
    }
}

/* ---------- 老配置升级 (缺键自动补齐) ---------- */

// 追加内容到已存在文件末尾。成功返回 TRUE。
static BOOL appendFile(const wchar_t *path, const char *data, DWORD len)
{
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return FALSE;
    SetFilePointer(h, 0, NULL, FILE_END);
    DWORD wrote = 0;
    BOOL ok     = WriteFile(h, data, len, &wrote, NULL);
    CloseHandle(h);
    return ok && wrote == len;
}

// 文本中是否存在某个键 (忽略注释行, 大小写不敏感)。
static BOOL iniHasKey(const char *text, const char *key)
{
    const char *line = text;
    while (line && *line)
    {
        const char *nl = strpbrk(line, "\r\n");
        const char *p  = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p && *p != ';' && *p != '#' && *p != '[')
        {
            // 只在本行范围内找 '=' (用 memchr 限界):
            // 若用 strchr 会一路扫到文件后方的 '=', 对无等号的行形成 O(n^2)。
            const char *end = nl ? nl : (p + strlen(p));
            const char *eq  = (const char *)memchr(p, '=', (size_t)(end - p));
            if (eq)
            {
                char keybuf[64];
                int n = 0;
                const char *q = p;
                while (q < eq && n < 63)
                    keybuf[n++] = *q++;
                keybuf[n] = 0;
                while (n > 0 && (keybuf[n - 1] == ' ' || keybuf[n - 1] == '\t'))
                    keybuf[--n] = 0;
                if (_stricmp(keybuf, key) == 0)
                    return TRUE;
            }
        }
        if (!nl)
            break;
        line = nl;
        while (*line == '\r' || *line == '\n')
            line++;
    }
    return FALSE;
}

/* ---------- 对外接口 ---------- */

BOOL bl_config_load(BlConfig *cfg)
{
    // 默认值 (配置文件缺失/键缺失时的回退)
    wcscpy_s(cfg->hotkey, BL_HOTKEY_MAX, L"Alt+L");
    cfg->password[0]         = 0;   // 默认空密码 (简单锁屏, 回车即解锁)
    cfg->autostart           = FALSE;
    cfg->require_password    = FALSE;
    cfg->path[0]             = 0;
    cfg->password_valid      = FALSE;
    cfg->security_downgraded = FALSE;

    wchar_t exeCfg[MAX_PATH], appCfg[MAX_PATH];
    BOOL haveExeCfg = pathExeConfig(exeCfg, MAX_PATH);

    // 1) exe 同目录优先 (取不到 exe 路径则跳过, 不猜相对路径)
    if (haveExeCfg && GetFileAttributesW(exeCfg) != INVALID_FILE_ATTRIBUTES)
    {
        wcscpy_s(cfg->path, MAX_PATH, exeCfg);
    }
    // 2) 回退 %APPDATA%\BlackLock
    else if (pathAppDataConfig(appCfg, MAX_PATH, FALSE) &&
             GetFileAttributesW(appCfg) != INVALID_FILE_ATTRIBUTES)
    {
        wcscpy_s(cfg->path, MAX_PATH, appCfg);
    }
    // 3) 都不存在 -> 只在 %APPDATA%\BlackLock 生成默认 (保持 exe 目录洁净;
    //    便携性由"可选的 exe 同目录 config.ini 优先级最高"保证)
    else
    {
        if (pathAppDataConfig(appCfg, MAX_PATH, TRUE) && writeDefaultConfig(appCfg))
            wcscpy_s(cfg->path, MAX_PATH, appCfg);
        else
            return FALSE; // 无法读取也无法生成
    }

    DWORD flen  = 0;
    char *text  = readFileAll(cfg->path, &flen);
    if (!text)
        return FALSE;
    // 注意: parseIni 会就地破坏 text (把 '=' 与换行替换为 '\0'),
    // 因此升级检测必须在解析之前、对原始文本进行。
    BOOL needMigrate = !iniHasKey(text, "require_password");

    parseIni(text, cfg);

    // 老配置升级: 若缺 require_password 键 (v1.1 新增), 自动补一段带注释的配置,
    // 让升级用户能在配置文件里看到并修改"开启密码"开关。
    if (needMigrate)
    {
        static const char *blk =
            "\r\n; 是否需要密码解锁 (v1.1 新增)。也可在托盘菜单勾选\"开启密码\"切换。\r\n"
            ";   false = 简单锁屏: 按回车或再次按解锁快捷键即可解锁 (无需密码)\r\n"
            ";   true  = 需输入上面的 password + 回车 才能解锁\r\n"
            "require_password = false\r\n";
        appendFile(cfg->path, blk, (DWORD)strlen(blk));
    }

    free(text);

    // 密码有效性: 非空 + 仅 ASCII 字母数字 (超长/非法 UTF-8 已在转换时置空, 一并被判无效)
    cfg->password_valid = bl_password_chars_ok(cfg->password);

    // 安全校验: 要求密码但没有有效密码时, 绝不能假装"已受密码保护"
    // (空密码 -> 空回车即解锁; 含 IME/emoji 等字符 -> 根本输不进去)。
    // 这里强制降级为"无需密码", 并标记 security_downgraded 由上层提示用户。
    if (cfg->require_password && !cfg->password_valid)
    {
        cfg->require_password    = FALSE;
        cfg->security_downgraded = TRUE;
    }

    return TRUE;
}

BOOL bl_config_save_bool(const BlConfig *cfg, const char *key, BOOL value)
{
    if (cfg->path[0] == 0)
        return FALSE;

    DWORD flen = 0;
    char *text = readFileAll(cfg->path, &flen);
    if (!text)
        return FALSE;

    const char *newVal = value ? "true" : "false";

    // 输出缓冲: 逐行拷贝, 命中目标键则替换其值行
    size_t cap = flen + 128;
    char *out  = (char *)malloc(cap);
    if (!out)
    {
        free(text);
        return FALSE;
    }
    size_t olen = 0;
    BOOL replaced = FALSE;

    char *line = text;
    while (line && *line)
    {
        char *nl    = strpbrk(line, "\r\n");
        char *lineEnd; // 指向行内容之后 (换行前)
        char *next;
        if (nl)
        {
            lineEnd = nl;
            next    = nl; // 保留原换行符, 一并拷贝
        }
        else
        {
            lineEnd = line + strlen(line);
            next    = NULL;
        }

        // 判断该行 (去左空白) 是否 autostart 键
        char saved = *lineEnd;
        *lineEnd   = 0;
        char *p    = ltrim(line);
        BOOL isKey = FALSE;
        if (!replaced && *p != ';' && *p != '#')
        {
            char *eq = strchr(p, '=');
            if (eq)
            {
                char tmp = *eq;
                *eq      = 0;
                char keybuf[64];
                lstrcpynA(keybuf, p, sizeof(keybuf)); // 截断安全拷贝 (Win32)
                rtrim(keybuf);
                *eq = tmp;
                if (_stricmp(keybuf, key) == 0)
                    isKey = TRUE;
            }
        }
        *lineEnd = saved;

        if (isKey)
        {
            int w = snprintf(out + olen, cap - olen, "%s = %s", key, newVal);
            if (w > 0)
                olen += (size_t)w;
            replaced = TRUE;
            // 跳过原值, 保留原换行符
            if (next)
            {
                while (*next == '\r' || *next == '\n')
                {
                    out[olen++] = *next;
                    next++;
                }
            }
        }
        else
        {
            size_t seg = (size_t)(lineEnd - line);
            memcpy(out + olen, line, seg);
            olen += seg;
            if (next)
            {
                while (*next == '\r' || *next == '\n')
                {
                    out[olen++] = *next;
                    next++;
                }
            }
        }
        line = next;
    }

    // 文件里没有该键 -> 追加一行
    if (!replaced)
    {
        int w = snprintf(out + olen, cap - olen, "%s = %s\r\n", key, newVal);
        if (w > 0)
            olen += (size_t)w;
    }

    BOOL ok = writeFileAtomic(cfg->path, out, (DWORD)olen);
    free(out);
    free(text);
    return ok;
}

BOOL bl_config_open_in_editor(const BlConfig *cfg)
{
    if (cfg->path[0] == 0)
        return FALSE;
    HINSTANCE r = ShellExecuteW(NULL, L"open", cfg->path, NULL, NULL, SW_SHOWNORMAL);
    return ((INT_PTR)r > 32);
}
