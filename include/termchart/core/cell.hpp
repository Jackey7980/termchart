// cell.hpp —— 终端字符单元
//
// 关键约定（破坏它等于破坏整个宽度模型）：
//   * Cell 只存**单个主码点**；组合记号绝不进 Cell。
//   * 组合序列（e.g. 'e' + U+0301）由 CellBuffer 的稀疏侧面表承载（side bearing），
//     数据结构定义在 core/grapheme.hpp，分割在 util/grapheme.hpp。
//   * 于是 Cell 是 trivially copyable：差分遍历时可 memcpy/memcmp。
#ifndef TERMCHART_CORE_CELL_HPP
#define TERMCHART_CORE_CELL_HPP

#include <cstdint>

#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart {

struct Cell {
    char32_t cp = U' ';
    Style style;

    constexpr Cell() noexcept = default;
    constexpr explicit Cell(char32_t codepoint) noexcept : cp(codepoint) {}
    constexpr Cell(char32_t codepoint, Style s) noexcept : cp(codepoint), style(s) {}
};

TERMCHART_API bool operator==(const Cell& lhs, const Cell& rhs) noexcept;
TERMCHART_API bool operator!=(const Cell& lhs, const Cell& rhs) noexcept;

// 宽字符（显示宽 2）占据两个网格单元：第一格放主码点，第二格放续接占位。
// 续接格**不产生任何输出字节**——终端绘制宽字形时自动占两列，
// 再补一个空格会覆盖字形右半边。生产者（api/quick、canvas）负责写续接格，
// 渲染器（render/renderer）负责跳过它。
inline constexpr char32_t kContinuationCp = U'\0';

constexpr bool is_continuation(const Cell& cell) noexcept {
    return cell.cp == kContinuationCp;
}

// 是否为"空白且无样式"：差分渲染时用于跳过。
TERMCHART_API bool is_blank(const Cell& cell) noexcept;

}  // namespace termchart

#endif  // TERMCHART_CORE_CELL_HPP
