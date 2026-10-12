#include "termchart/app/loop.hpp"

#include <chrono>
#include <utility>

#include "termchart/util/math.hpp"

namespace termchart::app {
namespace {

double default_clock() noexcept {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

Loop::Loop(ClockFn clock) : clock_(std::move(clock)) {}

double Loop::now() const noexcept {
    return clock_ ? clock_() : default_clock();
}

std::uint64_t Loop::add_tick(TickFn fn) {
    if (!fn) {
        return 0;
    }
    tickers_.push_back(Ticker{next_id_, std::move(fn)});
    return next_id_++;
}

bool Loop::remove_tick(std::uint64_t id) {
    for (std::size_t i = 0; i < tickers_.size(); ++i) {
        if (tickers_[i].id == id) {
            tickers_.erase(tickers_.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

void Loop::stop() noexcept { stopped_ = true; }

void Loop::set_max_delta(double seconds) noexcept {
    if (util::is_finite(seconds) && seconds > 0.0) {
        max_delta_ = seconds;
    }
}

std::uint64_t Loop::run(std::uint64_t max_iterations) {
    stopped_ = false;
    // 不重置 has_last_time_：跨 run() 的时间也要算 delta（先 run(1) 停顿
    // 再 run(n)，中间流逝的时间对动画是真实存在的）。
    std::uint64_t frames = 0;
    while (frames < max_iterations && !stopped_) {
        const double current = now();
        double delta = 0.0;
        if (has_last_time_) {
            delta = util::clamp<double>(current - last_time_, 0.0, max_delta_);
        }
        last_time_ = current;
        has_last_time_ = true;
        // 副本上迭代：回调内 stop()/remove_tick()/add_tick() 都不影响本帧。
        const std::vector<Ticker> snapshot = tickers_;
        for (const Ticker& ticker : snapshot) {
            if (ticker.fn) {
                ticker.fn(delta);
            }
        }
        ++frames;
        ++iterations_;
    }
    return frames;
}

}  // namespace termchart::app
