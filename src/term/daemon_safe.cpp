#include "termchart/term/daemon_safe.hpp"

namespace termchart::term {

DaemonSafeSettings daemon_safe_for(bool tty, bool ci) noexcept {
    DaemonSafeSettings safe;
    if (!tty) {
        // 非 tty：什么都不发。这是"日志不被污染"的唯一保证。
        safe.enabled = true;
        safe.allow_color = false;
        safe.allow_alt_screen = false;
        safe.allow_raw_mode = false;
        safe.allow_cursor_hiding = false;
        return safe;
    }
    if (ci) {
        // tty 但在 CI：保留结构，去掉颜色（golden 对比与管道消费更省事）。
        safe.enabled = true;
        safe.allow_color = false;
    }
    return safe;
}

TerminalCaps apply(const TerminalCaps& caps, const DaemonSafeSettings& safe) noexcept {
    TerminalCaps out = caps;
    if (!safe.allow_color) {
        out.color_depth = ColorDepth::None;
    }
    if (!safe.allow_alt_screen) {
        out.alt_screen = false;
    }
    return out;
}

TerminalOptions apply(const TerminalOptions& options, const DaemonSafeSettings& safe) noexcept {
    TerminalOptions out = options;
    if (!safe.allow_alt_screen) {
        out.alt_screen = false;
    }
    if (!safe.allow_raw_mode) {
        out.raw_mode = false;
    }
    if (!safe.allow_cursor_hiding) {
        out.hide_cursor = false;
    }
    if (!safe.allow_color) {
        // 颜色关了就不该开鼠标：交互输出本来也不可读。
        out.enable_mouse = false;
    }
    return out;
}

}  // namespace termchart::term
