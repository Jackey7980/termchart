// ansi.hpp —— ANSI 序列常量与生成
//
// 边界（重要，别在这里加东西）：
//   * 本文件只管 **光标/擦除/屏幕切换** 这类控制序列；
//   * 颜色与属性序列（SGR）的唯一生成点是 render/sgr.hpp —— 收窄边已登记，
//     任何别的模块都不许自己拼 ESC[...m。
// 序列常量表见 detail/ansi_tables.hpp。
#ifndef TERMCHART_TERM_ANSI_HPP
#define TERMCHART_TERM_ANSI_HPP

#include <string>
#include <string_view>

#include "termchart/export.hpp"

namespace termchart::term::ansi {

// ── 光标 ────────────────────────────────────────────────────────────────
TERMCHART_API void append_cursor_to(std::string& out, int row, int col) noexcept;  // 1-based
TERMCHART_API void append_cursor_up(std::string& out, int n = 1) noexcept;
TERMCHART_API void append_cursor_down(std::string& out, int n = 1) noexcept;
TERMCHART_API void append_cursor_forward(std::string& out, int n = 1) noexcept;
TERMCHART_API void append_cursor_back(std::string& out, int n = 1) noexcept;
TERMCHART_API void append_cursor_home(std::string& out) noexcept;

TERMCHART_API std::string cursor_to(int row, int col) noexcept;
TERMCHART_API std::string cursor_home() noexcept;

// ── 擦除 ────────────────────────────────────────────────────────────────
TERMCHART_API void append_erase_to_end_of_line(std::string& out) noexcept;
TERMCHART_API void append_erase_line(std::string& out) noexcept;
TERMCHART_API void append_erase_display(std::string& out) noexcept;
TERMCHART_API std::string erase_line() noexcept;

// ── 光标可见性 / 交替屏幕 ───────────────────────────────────────────────
TERMCHART_API std::string hide_cursor() noexcept;
TERMCHART_API std::string show_cursor() noexcept;
TERMCHART_API std::string enter_alt_screen() noexcept;
TERMCHART_API std::string exit_alt_screen() noexcept;

// 位置跳转：给定当前与目标行列，优先选用最短序列。
// 由 render/renderer.hpp 使用；别处自行拼制符会让差分输出失控。
TERMCHART_API void append_move(std::string& out, int from_row, int from_col, int to_row,
                               int to_col) noexcept;

}  // namespace termchart::term::ansi

#endif  // TERMCHART_TERM_ANSI_HPP
