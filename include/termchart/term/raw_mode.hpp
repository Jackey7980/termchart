// raw_mode.hpp —— 原始模式 RAII
//
// 关键约束：**即使抛异常也必须恢复终端**。因此这里只提供 RAII 一种用法，
// 不暴露裸 enable/disable 给用户——"忘记恢复"是这类 bug 的一半来源。
// 平台差异全部在 src/term/terminal_posix.cpp / terminal_win32.cpp，头文件无 #ifdef。
#ifndef TERMCHART_TERM_RAW_MODE_HPP
#define TERMCHART_TERM_RAW_MODE_HPP

#include "termchart/export.hpp"

namespace termchart::term {

class RawMode {
public:
    RawMode() = default;

    // 进入原始模式。失败时 active() == false（例如 stdin 不是 tty）。
    TERMCHART_API void enter() noexcept;

    // 显式退出；可多次调用（幂等）。
    TERMCHART_API void leave() noexcept;

    TERMCHART_API ~RawMode();

    RawMode(const RawMode&) = delete;
    RawMode& operator=(const RawMode&) = delete;

    // 移动：把"恢复责任"移交给新对象。
    RawMode(RawMode&& other) noexcept : active_(other.active_) { other.active_ = false; }
    RawMode& operator=(RawMode&& other) noexcept {
        if (this != &other) {
            leave();
            active_ = other.active_;
            other.active_ = false;
        }
        return *this;
    }

    [[nodiscard]] bool active() const noexcept { return active_; }

    // 测试探针：不进真实原始模式，只翻转内部标志（用于终端恢复回归测试）。
    static TERMCHART_API RawMode make_noop(bool pretend_active) noexcept;

private:
    explicit RawMode(bool pretend_active) noexcept : pretend_(true), active_(pretend_active) {}

    bool pretend_ = false;
    bool active_ = false;
};

}  // namespace termchart::term

#endif  // TERMCHART_TERM_RAW_MODE_HPP
