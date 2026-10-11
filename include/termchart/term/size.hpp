// size.hpp —— 终端尺寸查询与 SIGWINCH
//
// 尺寸是"终端说了算"的外部状态：任何时候都可能变（用户拉窗口）。
// 这里只提供**查询**；真正的循环怎么响应 resize 在 app/loop.hpp，
// 信号转发走 term/signal.hpp + term/waker.hpp（绝不进 signal handler 干活）。
#ifndef TERMCHART_TERM_SIZE_HPP
#define TERMCHART_TERM_SIZE_HPP

#include <cstdint>

#include "termchart/core/point.hpp"
#include "termchart/export.hpp"

namespace termchart::term {

struct TerminalSize {
    std::uint16_t cols = 80;
    std::uint16_t rows = 24;

    constexpr bool operator==(const TerminalSize&) const noexcept = default;
    constexpr bool operator!=(const TerminalSize&) const noexcept = default;
    [[nodiscard]] constexpr bool valid() const noexcept { return cols > 0 && rows > 0; }
    [[nodiscard]] constexpr Point as_point() const noexcept {
        return Point{static_cast<double>(cols), static_cast<double>(rows)};
    }
};

// 平台实现隔离在 src/term/terminal_posix.cpp / terminal_win32.cpp，
// 失败（非 tty / ioctl 拒绝）时回落到 80×24。
TERMCHART_API TerminalSize query_size() noexcept;

// 环境变量回退（COLUMNS / LINES）：CI 里没有 tty，这是唯一可用来源。
TERMCHART_API TerminalSize size_from_env() noexcept;

}  // namespace termchart::term

#endif  // TERMCHART_TERM_SIZE_HPP
