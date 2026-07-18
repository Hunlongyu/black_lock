// util.h —— 通用工具: 路径、编码转换、内存清零、SHA-256
#ifndef BLACKLOCK_UTIL_H
#define BLACKLOCK_UTIL_H

// clang-format off
#include <windows.h>
// clang-format on

// 安全清零 (防编译器优化掉)
void bl_secure_zero(void *p, size_t n);

// 获取当前 exe 所在目录 (末尾带反斜杠)。成功返回 TRUE。
BOOL bl_get_exe_dir(wchar_t *out, size_t cch);

// 获取当前 exe 完整路径。成功返回 TRUE。
BOOL bl_get_exe_path(wchar_t *out, size_t cch);

// 把路径转换为 Win32 文件 API 使用的扩展长度 (\\?\) 形式, 以突破 MAX_PATH 且不依赖系统策略。
// 先经 GetFullPathNameW 规范化 (折叠重复分隔符、解析 . / ..、/ 转 \、补全相对路径),
// 再加前缀: 盘符绝对 -> \\?\C:\...、UNC -> \\?\UNC\server\share。已是 \\?\ / \\.\ 的原样返回。
// 返回堆缓冲 (调用者 free), 失败返回 NULL。
wchar_t *bl_file_api_path(const wchar_t *path);

// 判断文件/目录是否存在 (内部经 bl_file_api_path 处理长路径)。
BOOL bl_file_exists(const wchar_t *path);

// UTF-16 -> UTF-8。返回写入字节数 (不含结尾 NUL); 失败返回 -1。
int bl_utf16_to_utf8(const wchar_t *w, int wlen, char *out, int outsz);

// 计算 data[0..len) 的 SHA-256 到 out[32]。成功返回 TRUE。
BOOL bl_sha256(const BYTE *data, ULONG len, BYTE out[32]);

#endif // BLACKLOCK_UTIL_H
