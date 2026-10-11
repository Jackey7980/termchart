// text.hpp —— 文本排版：截断、对齐、换行、填充
//
// 所有操作以**显示宽度**而非码点数为单位——这是终端排版与 Web 的根本差异之一。
// 因此全部依赖 util/wcwidth.hpp 与 util/grapheme.hpp。
#ifndef TERMCHART_UTIL_TEXT_HPP
#define TERMCHART_UTIL_TEXT_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/export.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::util {

enum class Align : std::uint8_t { Left, Center, Right };

// 重复字符 n 次：进度条、分隔线用。n == 0 返回空串。
TERMCHART_API std::string repeat(char32_t cp, std::size_t n);

// 宽度 ≤ max_width 的对齐填充。超宽时不截断（截断请用 truncate）。
TERMCHART_API std::string pad(std::string_view text, int width, Align align = Align::Left,
                              char32_t pad_char = U' ');

// 截断到 max_width；宽度不足时退化为省略符（默认 "..."）。
TERMCHART_API std::string truncate(std::string_view text, int max_width,
                                   Align align = Align::Left, std::string_view ellipsis = "...");

// 折行：贪心按簇边界断；超长单词（无空格）在 force_break 时硬切。
TERMCHART_API std::vector<std::string> wrap(std::string_view text, int width,
                                            bool force_break = true);

// 取显示宽度≤max_width 的前缀，并补齐到 max_width（标题栏、图例条常用）。
TERMCHART_API std::string fit(std::string_view text, int max_width, Align align = Align::Left);

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_TEXT_HPP
