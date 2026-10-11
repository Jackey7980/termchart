// terminal.hpp —— 终端 RAII 门面
//
// 唯一"拥有"终端状态的地方：
//   * 构造时不改动任何东西，调用 enter() 才真正进入；
//   * 析构（含异常栈展开）必定**按相反顺序**撤销 enter() 做过的每一件事；
//   * daemon_safe.hpp 负责"非 tty 时退行为"。
//
// 平台差异全部落在 src/term/terminal_posix.cpp 与 terminal_win32.cpp：
// **本头文件没有任何 #ifdef**（违反这条就是架构泄漏）。
//
// 返回值约定：只回报"成功与否"的操作返回 Status（即 ErrorCode）；
// 需要携带值的才用 Result<T>（见 core/expected.hpp）。
#ifndef TERMCHART_TERM_TERMINAL_HPP
#define TERMCHART_TERM_TERMINAL_HPP

#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/term/caps.hpp"
#include "termchart/term/raw_mode.hpp"
#include "termchart/term/size.hpp"

namespace termchart::term {

struct TerminalOptions {
    bool raw_mode = false;      // 关闭回显/行缓冲：仅交互场景需要
    bool alt_screen = true;     // 用交替屏幕：退出后自动回到原内容
    bool hide_cursor = true;    // 隐藏光标只在 enter 期间生效
    bool enable_mouse = false;  // 阶段 5 的交互需要
};

class Terminal {
public:
    explicit Terminal(TerminalOptions options = {}) noexcept;
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    Terminal(Terminal&& other) noexcept;
    Terminal& operator=(Terminal&& other) noexcept;

    // 进入：按 options 逐项启用。**记录已生效项**，供析构精确回滚。
    TERMCHART_API Status enter() noexcept;
    TERMCHART_API Status leave() noexcept;
    TERMCHART_API void restore() noexcept;  // leave() 的 noexcept 版，给析构/信号处理用

    [[nodiscard]] TERMCHART_API bool entered() const noexcept { return entered_; }
    [[nodiscard]] TERMCHART_API const TerminalOptions& options() const noexcept { return options_; }

    // 写入 / 立即生效：不使用 iostream，避免 locale 与缓冲策略差异。
    TERMCHART_API Status write(std::string_view bytes) noexcept;
    TERMCHART_API Status flush() noexcept;

    [[nodiscard]] TERMCHART_API TerminalSize size() const noexcept;
    [[nodiscard]] TERMCHART_API TerminalCaps caps() const noexcept;

    // ── 静态探测 ──────────────────────────────────────────────────────
    [[nodiscard]] static TERMCHART_API bool stdout_is_tty() noexcept;
    [[nodiscard]] static TERMCHART_API bool stdin_is_tty() noexcept;

private:
    struct Changes {
        bool alt_screen = false;
        bool cursor_hidden = false;
        bool mouse_enabled = false;
    };

    TerminalOptions options_{};
    RawMode raw_mode_;
    Changes changes_{};
    bool entered_ = false;
    TerminalCaps caps_{};
};

}  // namespace termchart::term

#endif  // TERMCHART_TERM_TERMINAL_HPP
