// timer.cpp —— 计时实现
#include "termchart/util/timer.hpp"

#include <algorithm>
#include <chrono>

namespace termchart::util {

TimePoint monotonic_start() noexcept {
    // function-local static：进程内唯一的单调起点（首次调用时确定）。
    static const TimePoint start = Clock::now();
    return start;
}

TimePoint now() noexcept {
    return Clock::now();
}

double seconds_since_start() noexcept {
    return std::chrono::duration<double>(now() - monotonic_start()).count();
}

void Stopwatch::restart() noexcept {
    start_ = now();
}

double Stopwatch::elapsed_ms() const noexcept {
    return std::chrono::duration<double, std::milli>(now() - start_).count();
}

double Stopwatch::elapsed_seconds() const noexcept {
    return std::chrono::duration<double>(now() - start_).count();
}

FrameTimer::FrameTimer(double smoothing) noexcept
    : smoothing_(std::clamp(smoothing, 0.0, 0.999)) {}

double FrameTimer::tick() noexcept {
    return tick_at(now());
}

double FrameTimer::tick_at(TimePoint stamp) noexcept {
    ++frames_;
    if (!started_) {
        started_ = true;
        last_ = stamp;
        delta_ = 0.0;
        return 0.0;  // 首帧没有"上一帧"
    }
    const double raw = std::chrono::duration<double>(stamp - last_).count();
    last_ = stamp;
    // 时间源被换过（或注入的测试时间回退）会产生负间隔：夹到 0，
    // 否则 fps 会是负数，调用方拿去做插值会算出反向动画。
    const double gap = raw > 0.0 ? raw : 0.0;
    delta_ = delta_ <= 0.0 ? gap : smoothing_ * delta_ + (1.0 - smoothing_) * gap;
    return gap;  // 返回最近一帧的原始间隔；平滑值看 delta_seconds()
}

double FrameTimer::fps() const noexcept {
    return delta_ > 0.0 ? 1.0 / delta_ : 0.0;
}

void FrameTimer::reset() noexcept {
    delta_ = 0.0;
    frames_ = 0;
    started_ = false;
}

void Timeout::restart() noexcept {
    start_ = now();
}

void Timeout::restart_at(TimePoint stamp) noexcept {
    start_ = stamp;
}

bool Timeout::expired_at(TimePoint stamp) const noexcept {
    return (stamp - start_) >= limit_;
}

bool Timeout::expired() const noexcept {
    return expired_at(now());
}

double Timeout::remaining_seconds(TimePoint stamp) const noexcept {
    const double left = std::chrono::duration<double>(limit_ - (stamp - start_)).count();
    return left > 0.0 ? left : 0.0;
}

}  // namespace termchart::util
