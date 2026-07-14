// util.c —— 通用工具实现
// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on
#include <string.h>
#include <wchar.h>
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
