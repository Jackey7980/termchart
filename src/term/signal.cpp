// signal.cpp —— 信号转发实现
//
// 平台差异只在本文件。信号处理器里只做两件事：置 atomic 标志 + notify waker。
#if !defined(_WIN32)

#include <signal.h>

#include <atomic>

#include "termchart/term/signal.hpp"

namespace termchart::term {
namespace {

std::atomic<bool> g_resize_flag{false};
std::atomic<Waker*> g_waker{nullptr};
struct sigaction g_previous_action {};
bool g_installed = false;

void on_sigwinch(int) noexcept {
    g_resize_flag.store(true, std::memory_order_release);
    Waker* waker = g_waker.load(std::memory_order_acquire);
    if (waker != nullptr) {
        waker->notify();
    }
}

}  // namespace

bool install_resize_forwarding(Waker& waker) noexcept {
    g_waker.store(&waker, std::memory_order_release);
    if (g_installed) {
        return true;
    }
    struct sigaction action {};
    action.sa_handler = &on_sigwinch;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;  // 不让信号打断 read()，主线程逻辑更简单
    if (sigaction(SIGWINCH, &action, &g_previous_action) != 0) {
        g_waker.store(nullptr, std::memory_order_release);
        return false;
    }
    g_installed = true;
    return true;
}

void uninstall_resize_forwarding() noexcept {
    if (!g_installed) {
        return;
    }
    (void)sigaction(SIGWINCH, &g_previous_action, nullptr);
    g_installed = false;
    g_waker.store(nullptr, std::memory_order_release);
}

bool take_resize_flag() noexcept {
    return g_resize_flag.exchange(false, std::memory_order_acq_rel);
}

void simulate_resize_for_test() noexcept { on_sigwinch(SIGWINCH); }

}  // namespace termchart::term

#else  // Windows：无 SIGWINCH，转发动作退化为"只记标志"

#include <atomic>

#include "termchart/term/signal.hpp"

namespace termchart::term {
namespace {

std::atomic<bool> g_resize_flag{false};
std::atomic<Waker*> g_waker{nullptr};

}  // namespace

bool install_resize_forwarding(Waker& waker) noexcept {
    g_waker.store(&waker, std::memory_order_release);
    return true;
}

void uninstall_resize_forwarding() noexcept {
    g_waker.store(nullptr, std::memory_order_release);
}

bool take_resize_flag() noexcept { return g_resize_flag.exchange(false, std::memory_order_acq_rel); }

void simulate_resize_for_test() noexcept {
    g_resize_flag.store(true, std::memory_order_release);
    Waker* waker = g_waker.load(std::memory_order_acquire);
    if (waker != nullptr) {
        waker->notify();
    }
}

}  // namespace termchart::term

#endif
