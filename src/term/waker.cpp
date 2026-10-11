// waker.cpp —— 跨线程唤醒门面
//
// 只做一件事：把平台原语包成 RAII + 移动语义。
// 语义保证：notify() 之后至少一次 wait() 会返回 true（不丢唤醒）。
#include "termchart/term/waker.hpp"

#include "src/term/platform_internal.hpp"

namespace termchart::term {

Waker::Waker() noexcept : native_(platform::create_waker()) {}

Waker::~Waker() {
    if (native_ != nullptr) {
        platform::destroy_waker(native_);
        native_ = nullptr;
    }
}

Waker::Waker(Waker&& other) noexcept : native_(other.native_) { other.native_ = nullptr; }

Waker& Waker::operator=(Waker&& other) noexcept {
    if (this != &other) {
        if (native_ != nullptr) {
            platform::destroy_waker(native_);
        }
        native_ = other.native_;
        other.native_ = nullptr;
    }
    return *this;
}

void Waker::notify() noexcept { platform::notify_waker(native_); }

bool Waker::wait(int timeout_ms) noexcept { return platform::wait_waker(native_, timeout_ms); }

void Waker::drain() noexcept { platform::drain_waker(native_); }

std::uintptr_t Waker::native_handle() const noexcept {
    return platform::waker_native_handle(native_);
}

}  // namespace termchart::term
