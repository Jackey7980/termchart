// file.hpp —— 以 UTF-8 路径打开文件
//
// 为什么需要这一层：Windows 上 `std::fopen(path, ...)` 的窄字符版本按**当前 ANSI
// 代码页**解释路径，而库内路径一律是 UTF-8。用户名含中文时（`C:\Users\琳琳\...`）
// 直接 fopen 必然失败——调用方明明传了正确路径，却拿到 IoError。
//
// 收口策略：所有「按调用方给的路径读写」都走 fopen_utf8，平台差异只落在
// src/util/file_win32.cpp / file_posix.cpp，公开头不出现任何 #ifdef。
#ifndef TERMCHART_UTIL_FILE_HPP
#define TERMCHART_UTIL_FILE_HPP

#include <cstdio>
#include <string>

#include "termchart/export.hpp"

namespace termchart::util {

// 以 UTF-8 编码的 path 打开文件；mode 语义与 std::fopen 完全一致（"rb" / "wb" ...）。
// 返回 nullptr 表示打不开，调用方按原有逻辑处理即可。
TERMCHART_API std::FILE* fopen_utf8(const std::string& path, const char* mode);

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_FILE_HPP
