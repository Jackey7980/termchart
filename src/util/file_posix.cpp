// file_posix.cpp —— POSIX 下以 UTF-8 路径打开文件
//
// 隔离层：本文件允许出现 #ifdef。
// POSIX 文件名的字节串原样使用，UTF-8 天然可用，无需转码。
#if !defined(_WIN32)

#include <cstdio>
#include <string>

#include "termchart/util/file.hpp"

namespace termchart::util {
namespace detail {
bool open_args_valid(const std::string& path, const char* mode) noexcept;
}  // namespace detail

std::FILE* fopen_utf8(const std::string& path, const char* mode) {
    if (!detail::open_args_valid(path, mode)) return nullptr;
    return std::fopen(path.c_str(), mode);
}

}  // namespace termchart::util

#endif  // !defined(_WIN32)
