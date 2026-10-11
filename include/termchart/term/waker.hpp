// waker.hpp —— 跨线程唤醒原语
//
// 并发模型（docs/design/concurrency.md）：
//   单线程主循环 + select/poll。后台线程**不得**触碰任何绘制对象，
//   只能通过 app/post.hpp 投递数据，然后 notify() 唤醒主线程。
// POSIX 用 self-pipe，Windows 用手动重置 event —— 差异封在实现里，头文件无 #ifdef。
#ifndef TERMCHART_TERM_WAKER_HPP
#define TERMCHART_TERM_WAKER_HPP

#include <cstdint>

#include "termchart/export.hpp"

namespace termchart::term {

class Waker {
public:
    TERMCHART_API Waker() noexcept;
    TERMCHART_API ~Waker();

    Waker(const Waker&) = delete;
    Waker& operator=(const Waker&) = delete;

    // 移动：所有权转移，"可被 select 的那个 fd/handle"跟着走。
    TERMCHART_API Waker(Waker&& other) noexcept;
    TERMCHART_API Waker& operator=(Waker&& other) noexcept;

    // 唤醒一次。可重入：信号处理里调用是安全的。
    TERMCHART_API void notify() noexcept;

    // 阻塞等待被唤醒；timeout_ms < 0 表示无限等待。返回是否由 notify 唤醒（false = 超时）。
    TERMCHART_API bool wait(int timeout_ms) noexcept;

    // 消费已排队的唤醒信号（不清空则下次 wait 立即返回）。
    TERMCHART_API void drain() noexcept;

    [[nodiscard]] TERMCHART_API bool valid() const noexcept { return native_ != nullptr; }

    // 给主循环的 select/poll；Windows 返回 HANDLE，POSIX 返回读端 fd。
    // **平台差异在此收口**：调用方只做转发，不解释类型。
    [[nodiscard]] TERMCHART_API std::uintptr_t native_handle() const noexcept;

private:
    void* native_ = nullptr;
};

}  // namespace termchart::term

#endif  // TERMCHART_TERM_WAKER_HPP
