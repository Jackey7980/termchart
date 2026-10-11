// platform_internal.hpp —— 终端平台层内部接口（**仅 src/term 内可见**）
//
// 存在理由：term/ 的头文件必须保持"零 #ifdef"（见 docs/design/layering.md），
// 于是平台差异只能封在 .cpp；而这些 .cpp 之间需要一个共享的、不导出的签名表。
//
// 这是 src/ 里唯一不对应 include/ 同名头的文件——**故意的例外**，
// 见 ROADMAP"平台隔离"纪律。不要把它移到 include/ 下。
#ifndef TERMCHART_SRC_TERM_PLATFORM_INTERNAL_HPP
#define TERMCHART_SRC_TERM_PLATFORM_INTERNAL_HPP

#include <cstdint>

namespace termchart::term::platform {

struct SizeResult {
    bool ok = false;
    std::uint16_t cols = 0;
    std::uint16_t rows = 0;
};

// 尺寸查询（ioctl / GetConsoleScreenBufferInfo）。失败返回 ok == false。
SizeResult query_terminal_size() noexcept;

// 原始模式：实现内部需保存原始终端属性，以便精确恢复。
bool enable_raw_mode() noexcept;
bool disable_raw_mode() noexcept;

bool stdout_is_tty() noexcept;
bool stdin_is_tty() noexcept;

// VT（虚拟终端）处理：Windows 控制台默认**不解释** ANSI 转义序列，
// 需显式开启 ENABLE_VIRTUAL_TERMINAL_PROCESSING，否则 render/sgr.hpp 产出的
// SGR / 光标 / 交替屏序列会作为普通字节裸露显示。POSIX 终端原生支持，空实现。
// 实现需保存进入前的 mode 以便精确恢复（原本就开着 VT 的终端保持开启）。
bool enable_vt() noexcept;
bool disable_vt() noexcept;

// ── Waker 平台原语 ──────────────────────────────────────────────────────
void* create_waker() noexcept;                 // 失败返回 nullptr
void destroy_waker(void* handle) noexcept;
void notify_waker(void* handle) noexcept;
bool wait_waker(void* handle, int timeout_ms) noexcept;  // true = 被唤醒；false = 超时
void drain_waker(void* handle) noexcept;
std::uintptr_t waker_native_handle(void* handle) noexcept;

}  // namespace termchart::term::platform

#endif  // TERMCHART_SRC_TERM_PLATFORM_INTERNAL_HPP
