// timer.hpp —— 单调时钟、秒表、帧计时、超时判断
//
// 全部基于 `std::chrono::steady_clock`：它单调递增，不受系统时间调整影响，
// 适合测耗时；而"墙上时间"应当用 system_clock，本模块不掺和。
//
// 可测试性：帧计时与超时都提供**注入时间点**的接口（tick_at / expired_at），
// 单测因此不依赖真实时间流逝（也不会因为机器慢而抖动）。
#ifndef TERMCHART_UTIL_TIMER_HPP
#define TERMCHART_UTIL_TIMER_HPP

#include <chrono>
#include <cstdint>

#include "termchart/export.hpp"

namespace termchart::util {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

// 进程内单调起点（首次调用时确定），用于打印相对时间。
[[nodiscard]] TERMCHART_API TimePoint monotonic_start() noexcept;

// 当前单调时刻。
[[nodiscard]] TERMCHART_API TimePoint now() noexcept;

// 距进程内单调起点的秒数（可用于日志时间戳）。
[[nodiscard]] TERMCHART_API double seconds_since_start() noexcept;

// 秒表：start / stop / reset，可反复读。
class Stopwatch {
public:
    Stopwatch() noexcept : start_(now()) {}

    void restart() noexcept;
    // 从上次 restart（或构造）到"现在"经过的毫秒 / 秒数。
    [[nodiscard]] double elapsed_ms() const noexcept;
    [[nodiscard]] double elapsed_seconds() const noexcept;

private:
    TimePoint start_;
};

// 帧计时：给出相邻两帧的间隔（可选指数平滑）与 FPS。
class FrameTimer {
public:
    // smoothing ∈ [0,1)：0 表示不平滑（用最近一帧），越接近 1 越平滑。
    explicit FrameTimer(double smoothing = 0.9) noexcept;

    // 推进一帧，返回距上一帧的秒数；首帧返回 0。
    double tick() noexcept;
    // 用指定时间推进（测试用，避免依赖真实时钟）。
    double tick_at(TimePoint stamp) noexcept;

    [[nodiscard]] double delta_seconds() const noexcept { return delta_; }
    // delta 为 0 时返回 0（不返回无穷）。
    [[nodiscard]] double fps() const noexcept;
    [[nodiscard]] std::uint64_t frame_count() const noexcept { return frames_; }

    void reset() noexcept;

private:
    double smoothing_ = 0.9;
    double delta_ = 0.0;
    std::uint64_t frames_ = 0;
    TimePoint last_{};
    bool started_ = false;
};

// 超时判断：构造时给定阈值，之后用 expired_at 查询（或用 expired 走真实时钟）。
class Timeout {
public:
    explicit Timeout(Duration limit) noexcept : limit_(limit), start_(now()) {}

    void restart() noexcept;
    void restart_at(TimePoint stamp) noexcept;
    [[nodiscard]] bool expired_at(TimePoint stamp) const noexcept;
    [[nodiscard]] bool expired() const noexcept;
    [[nodiscard]] double remaining_seconds(TimePoint stamp) const noexcept;
    [[nodiscard]] Duration limit() const noexcept { return limit_; }

private:
    Duration limit_{};
    TimePoint start_{};
};

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_TIMER_HPP
