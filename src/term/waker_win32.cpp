// waker_win32.cpp —— Waker 的 Windows 实现
//
// 用手动重置事件（manual-reset event）：notify 后保持有信号，直到主线程 drain。
// 这样"唤醒丢失"不可能发生——POSIX self-pipe 语义相同。
#if defined(_WIN32)

#include <windows.h>

#include "src/term/platform_internal.hpp"

namespace termchart::term::platform {

void* create_waker() noexcept {
    const HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    return event == nullptr ? nullptr : event;
}

void destroy_waker(void* handle) noexcept {
    if (handle != nullptr) {
        CloseHandle(handle);
    }
}

void notify_waker(void* handle) noexcept {
    if (handle != nullptr) {
        SetEvent(handle);
    }
}

bool wait_waker(void* handle, int timeout_ms) noexcept {
    if (handle == nullptr) {
        return false;
    }
    const DWORD wait = timeout_ms < 0 ? INFINITE : static_cast<DWORD>(timeout_ms);
    const DWORD result = WaitForSingleObject(handle, wait);
    return result == WAIT_OBJECT_0;
}

void drain_waker(void* handle) noexcept {
    if (handle != nullptr) {
        ResetEvent(handle);
    }
}

std::uintptr_t waker_native_handle(void* handle) noexcept {
    return reinterpret_cast<std::uintptr_t>(handle);
}

}  // namespace termchart::term::platform

#endif  // defined(_WIN32)
