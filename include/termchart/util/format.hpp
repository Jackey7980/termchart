// format.hpp —— 轻量整数 / 浮点格式化
//
// 为什么不用 iostream / std::format：
//   * iostream 受 locale 影响，同一份程序在不同机器上输出不同（golden 测试会炸）；
//   * std::format 依赖较新的标准库且体积大；项目只要"固定小数位 + 去尾零"两种。
// 因此这里用 <charconv> 的 to_chars：locale 无关、无分配、最快。
#ifndef TERMCHART_UTIL_FORMAT_HPP
#define TERMCHART_UTIL_FORMAT_HPP

#include <cstdint>
#include <string>

#include "termchart/core/rgb.hpp"
#include "termchart/export.hpp"

namespace termchart::util {

// 定点表示，precision 位小数；NaN/Inf 输出 "nan"/"inf"。
TERMCHART_API std::string format_double(double value, int precision = 2);

// 同上但去掉尾部多余的 0 与小数点：0.50 → "0.5"，1.000 → "1"。
TERMCHART_API std::string format_double_trimmed(double value, int precision = 2);

// 追加到已有缓冲：渲染热路径避免临时字符串。
TERMCHART_API void append_double(std::string& out, double value, int precision = 2);

// 千分位由调用方自行拼装；这里只提供无 locale 依赖的整数输出。
TERMCHART_API std::string format_int(std::int64_t value);

// 轴标签常用：2000 → "2k"，避免长数字挤占坐标空间。
TERMCHART_API std::string format_compact(double value, int precision = 1);

// 颜色序列化："#RRGGBB"（大写，7 字节）。调试日志、JSON、CSS 输出共用一种写法，
// 让 1.55 万的 Rgb 调用点不必各自拼 hex。Rgba 不在这里——alpha 怎么表示是
// 各导出格式自己的事。
TERMCHART_API std::string to_hex_string(const Rgb& color);

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_FORMAT_HPP
