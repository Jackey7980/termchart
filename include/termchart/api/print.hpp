// print.hpp —— stdout 输出封装
//
// 为什么单独一个文件：输出是**唯一会触碰进程全局状态的动作**。
// 把它收在一处，headless / CI / 测试就能整体替换（或直接不调用）。
// 约定：不做 locale 相关转换；写的是渲染管线产出的原始字节。
#ifndef TERMCHART_API_PRINT_HPP
#define TERMCHART_API_PRINT_HPP

#include <cstdio>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"

namespace termchart::api {

// 写入 FILE*（默认 stdout）：短写返回 IoError。
TERMCHART_API Status write_to(std::FILE* stream, std::string_view bytes) noexcept;
TERMCHART_API Status write_stdout(std::string_view bytes) noexcept;
TERMCHART_API Status write_stderr(std::string_view bytes) noexcept;

// 刷缓冲：终端交互下必须显式调用，否则输出会粘在缓冲区里。
TERMCHART_API Status flush(std::FILE* stream) noexcept;
TERMCHART_API Status flush_stdout() noexcept;

// 二进制安全诊断：把不可打印字节变成可读形式，供日志/golden 失败信息使用。
TERMCHART_API std::string escape_for_debug(std::string_view bytes);

}  // namespace termchart::api

#endif  // TERMCHART_API_PRINT_HPP
