// loop.hpp —— 主事件循环
//
// 节奏由**注入时钟**驱动（缺省 steady_clock 秒，同 util/timer.hpp 的理由）：
// 每帧先读时钟算 delta（首帧 delta = 0），再依次调用 tick 回调。
//
// delta 会被钳制到 max_delta（掉帧保护）：一次 10 秒的卡顿不应该让动画
// 直接跳到终点。上限缺省 0.1 秒，可用 set_max_delta 调整（非有限或 <=0
// 的值被忽略——"不限速"不是合法配置，那等于关掉保护）。
//
// tick 回调里可以安全调用 stop() 与 remove_tick()：每帧在回调列表的副本上
// 迭代，增删不影响当前帧。帧数极小（每秒几十），副本开销可忽略。
#ifndef TERMCHART_APP_LOOP_HPP
#define TERMCHART_APP_LOOP_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::app {

class Loop {
public:
    // tick(delta_seconds)：delta 为本帧距上一帧的秒数（首帧 0，已钳制）。
    using TickFn = std::function<void(double delta_seconds)>;
    // 时钟：当前时刻（秒），可注入。
    using ClockFn = std::function<double()>;

    explicit Loop(ClockFn clock = ClockFn{});

    // 登记每帧回调，返回 id（从 1 起）；空回调被忽略并返回 0。
    TERMCHART_API std::uint64_t add_tick(TickFn fn);

    // 返回是否真的删了（不存在 → false）。
    TERMCHART_API bool remove_tick(std::uint64_t id);

    // 停止标志：当前 run() 立即（或在本帧回调后）退出，可从 tick 内调用。
    TERMCHART_API void stop() noexcept;

    // 最多跑 max_iterations 帧，返回实际帧数。max_iterations = 0 → 0。
    // 每次调用 run() 都会先清除 stop 标志。
    TERMCHART_API std::uint64_t run(std::uint64_t max_iterations);

    [[nodiscard]] TERMCHART_API double max_delta() const noexcept { return max_delta_; }
    TERMCHART_API void set_max_delta(double seconds) noexcept;

    // 累计帧数（跨多次 run()）。
    [[nodiscard]] std::uint64_t iterations() const noexcept { return iterations_; }

private:
    [[nodiscard]] double now() const noexcept;

    ClockFn clock_;
    struct Ticker {
        std::uint64_t id;
        TickFn fn;
    };
    std::vector<Ticker> tickers_;
    std::uint64_t next_id_ = 1;
    std::uint64_t iterations_ = 0;
    double max_delta_ = 0.1;
    double last_time_ = 0.0;
    bool has_last_time_ = false;
    bool stopped_ = false;
};

}  // namespace termchart::app

#endif  // TERMCHART_APP_LOOP_HPP
