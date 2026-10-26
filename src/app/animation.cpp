#include "termchart/app/animation.hpp"

#include <cmath>
#include <chrono>

#include "termchart/util/math.hpp"

namespace termchart::app {
namespace {

double default_clock() noexcept {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

Animation::Animation(double duration_seconds, Easing easing, AnimationClock clock)
    : duration_(util::is_finite(duration_seconds) && duration_seconds > 0.0 ? duration_seconds : 0.0),
      easing_(easing),
      clock_(std::move(clock)) {}

double Animation::now() const noexcept {
    return clock_ ? clock_() : default_clock();
}

void Animation::start() noexcept {
    start_at(now());
}

void Animation::start_at(double now_seconds) noexcept {
    start_time_ = util::is_finite(now_seconds) ? now_seconds : 0.0;
    started_ = true;
}

double Animation::value_at(double now_seconds) const noexcept {
    if (!started_) {
        return 0.0;
    }
    if (duration_ == 0.0) {
        return 1.0;  // 零时长：开始即完成。
    }
    if (!util::is_finite(now_seconds)) {
        return 0.0;
    }
    const double elapsed = now_seconds - start_time_;
    if (elapsed <= 0.0) {
        return 0.0;
    }
    return ease(easing_, elapsed / duration_);
}

double Animation::value() noexcept {
    return value_at(now());
}

bool Animation::finished_at(double now_seconds) const noexcept {
    return value_at(now_seconds) >= 1.0;
}

bool Animation::finished() noexcept {
    return finished_at(now());
}

}  // namespace termchart::app
