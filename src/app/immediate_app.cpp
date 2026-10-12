#include "termchart/app/immediate_app.hpp"

#include <utility>

namespace termchart::app {

ImmediateApp::ImmediateApp(std::uint32_t width, std::uint32_t height, FrameFn frame,
                           Loop::ClockFn clock)
    : session_(width, height), loop_(std::move(clock)), frame_(std::move(frame)) {
    // 回调列表是常驻的：run() 多次调用共用同一个 tick。
    loop_.add_tick([this](double delta) { do_frame(delta); });
}

std::uint64_t ImmediateApp::run(std::uint64_t max_frames) {
    return loop_.run(max_frames);
}

void ImmediateApp::stop() noexcept { loop_.stop(); }

void ImmediateApp::do_frame(double delta_seconds) {
    // 即时模式三段式：清 → 画 → 收。
    session_.canvas().clear();
    if (frame_) {
        frame_(session_.canvas(), delta_seconds);
    }
    frames_.push_back(session_.frame());
}

}  // namespace termchart::app
