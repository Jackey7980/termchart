// file_win32.cpp —— Windows 下以 UTF-8 路径打开文件
//
// 隔离层：本文件允许出现 #ifdef 与 <windows.h>。
// UTF-8 → UTF-16 后走 _wfopen；模式串是纯 ASCII，逐字符扩宽即可。
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <string>

#include "termchart/util/file.hpp"

namespace termchart::util {
namespace detail {
bool open_args_valid(const std::string& path, const char* mode) noexcept;
}  // namespace detail

std::FILE* fopen_utf8(const std::string& path, const char* mode) {
    if (!detail::open_args_valid(path, mode)) return nullptr;

    const int wide_len = ::MultiByteToWideChar(CP_UTF8, 0, path.data(),
                                               static_cast<int>(path.size()), nullptr, 0);
    if (wide_len <= 0) return nullptr;  // 非法 UTF-8
    std::wstring wide_path(static_cast<std::size_t>(wide_len), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, path.data(), static_cast<int>(path.size()),
                          wide_path.data(), wide_len);

    // 模式串只含 "r/w/b/+/x" 等 ASCII 字符。
    std::wstring wide_mode;
    for (const char* p = mode; *p != '\0'; ++p) {
        wide_mode.push_back(static_cast<wchar_t>(*p));
    }
    return ::_wfopen(wide_path.c_str(), wide_mode.c_str());
}

}  // namespace termchart::util

#endif  // defined(_WIN32)
