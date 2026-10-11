// daemon_safe.hpp —— 非 tty 安全模式
//
// 场景：库被 cron / systemd / CI / 管道用。此时若照常发 ANSI 序列，
// 日志里会留下 \x1b[2J 之类的噪声，而用户只会反馈"输出脏了"。
//
// 策略（与本文件同名的 docs 节点保持一致）：
//   * 非 tty → 不发颜色、不进交替屏幕、不隐藏光标、不进原始模式；
//   * CI 明确要求"可预期输出"时，进一步把颜色档位打到 None。
#ifndef TERMCHART_TERM_DAEMON_SAFE_HPP
#define TERMCHART_TERM_DAEMON_SAFE_HPP

#include "termchart/export.hpp"
#include "termchart/term/caps.hpp"
#include "termchart/term/terminal.hpp"

namespace termchart::term {

struct DaemonSafeSettings {
    bool enabled = false;      // 是否处于"非 tty 安全"状态
    bool allow_color = true;   // 允许颜色（tty 时）
    bool allow_alt_screen = true;
    bool allow_raw_mode = true;
    bool allow_cursor_hiding = true;
};

// 依据"是否 tty + 是否 CI" 合成设置。纯函数，便于测试。
TERMCHART_API DaemonSafeSettings daemon_safe_for(bool tty, bool ci) noexcept;

// 把安全设置叠加到能力上（降级而非报错）。
TERMCHART_API TerminalCaps apply(const TerminalCaps& caps,
                                 const DaemonSafeSettings& safe) noexcept;

// 把安全设置叠加到终端选项上。**返回新值**，不改动入参：调用方需要能看出差异。
TERMCHART_API TerminalOptions apply(const TerminalOptions& options,
                                    const DaemonSafeSettings& safe) noexcept;

}  // namespace termchart::term

#endif  // TERMCHART_TERM_DAEMON_SAFE_HPP
