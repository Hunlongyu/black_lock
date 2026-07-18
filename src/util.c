// util.c —— 通用工具实现
// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "app.h"
#include "util.h"

void bl_secure_zero(void *p, size_t n)
{
    SecureZeroMemory(p, n);
}

BOOL bl_get_exe_path(wchar_t *out, size_t cch)
{
    DWORD n = GetModuleFileNameW(NULL, out, (DWORD)cch);
    return (n > 0 && n < cch);
}

BOOL bl_get_exe_dir(wchar_t *out, size_t cch)
{
    if (!bl_get_exe_path(out, cch))
        return FALSE;
    wchar_t *slash = wcsrchr(out, L'\\');
    if (!slash)
        return FALSE;
    slash[1] = 0; // 保留末尾反斜杠
    return TRUE;
}

wchar_t *bl_file_api_path(const wchar_t *path)
{
    if (!path || !path[0])
        return NULL;

    // 已是扩展长度形式 (\\?\ 或 \\.\): 原样返回堆副本。
    // 这类路径本就绕过内核规范化, 不能再喂给 GetFullPathNameW (它对其处理不可靠)。
    if (wcsncmp(path, L"\\\\?\\", 4) == 0 || wcsncmp(path, L"\\\\.\\", 4) == 0)
    {
        size_t n     = wcslen(path);
        wchar_t *dup = (wchar_t *)malloc((n + 1) * sizeof(wchar_t));
        if (dup)
            memcpy(dup, path, (n + 1) * sizeof(wchar_t));
        return dup;
    }

    // 关键: 加 \\?\ 前缀会关闭内核对路径的规范化, 之后 "\\" 双分隔符、"." / ".." 段
    // 都不再被折叠/解析, 会让原本合法的路径变非法 (如 APPDATA 带尾部反斜杠 -> 拼出
    // "...Roaming\\BlackLock")。因此先用 GetFullPathNameW 规范化 (折叠重复分隔符、
    // 解析 . / ..、/ 转 \、补全相对路径), 再加前缀。
    DWORD need = GetFullPathNameW(path, 0, NULL, NULL); // 返回所需字符数 (含结尾 NUL)
    if (need == 0 || need > BL_PATH_MAX)
        return NULL;
    wchar_t *full = (wchar_t *)malloc(need * sizeof(wchar_t));
    if (!full)
        return NULL;
    DWORD got = GetFullPathNameW(path, need, full, NULL); // 返回写入字符数 (不含 NUL)
    if (got == 0 || got >= need)
    {
        free(full);
        return NULL;
    }

    // 规范化后只会是 盘符绝对 (C:\...) 或 UNC (\\server\share)
    const wchar_t *prefix = L"";
    size_t prefixLen      = 0;
    size_t skip           = 0;
    if (wcsncmp(full, L"\\\\", 2) == 0) // UNC -> \\?\UNC\server\share
    {
        prefix    = L"\\\\?\\UNC\\";
        prefixLen = 8;
        skip      = 2;
    }
    else if (full[1] == L':' && full[2] == L'\\') // C:\... -> \\?\C:\...
    {
        prefix    = L"\\\\?\\";
        prefixLen = 4;
    }

    size_t outLen = prefixLen + (size_t)got - skip;
    if (outLen >= BL_PATH_MAX)
    {
        free(full);
        return NULL;
    }
    wchar_t *out = (wchar_t *)malloc((outLen + 1) * sizeof(wchar_t));
    if (!out)
    {
        free(full);
        return NULL;
    }
    if (prefixLen)
        memcpy(out, prefix, prefixLen * sizeof(wchar_t));
    memcpy(out + prefixLen, full + skip, ((size_t)got - skip + 1) * sizeof(wchar_t));
    free(full);
    return out;
}

BOOL bl_file_exists(const wchar_t *path)
{
    wchar_t *apiPath = bl_file_api_path(path);
    if (!apiPath)
        return FALSE;
    DWORD attr = GetFileAttributesW(apiPath);
    free(apiPath);
    return attr != INVALID_FILE_ATTRIBUTES;
}

int bl_utf16_to_utf8(const wchar_t *w, int wlen, char *out, int outsz)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, wlen, out, outsz, NULL, NULL);
    return (n <= 0) ? -1 : n;
}

BOOL bl_sha256(const BYTE *data, ULONG len, BYTE out[32])
{
    BCRYPT_ALG_HANDLE hAlg   = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    BOOL ok                  = FALSE;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0)
        return FALSE;
    if (BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) >= 0)
    {
        if (BCryptHashData(hHash, (PUCHAR)data, len, 0) >= 0 &&
            BCryptFinishHash(hHash, out, 32, 0) >= 0)
            ok = TRUE;
        BCryptDestroyHash(hHash);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}
