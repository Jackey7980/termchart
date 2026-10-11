// scheduler.hpp —— 单线程定时任务调度器
//
// 不建线程、不读时钟：**调用方每个 tick 传入"现在"**，调度器据此决定谁到期。
// 于是它是纯逻辑，可以确定性地测（配合 util/timer 的注入时间点），也不会出现
// "库内部偷偷起了个线程"这种事。线程与事件循环属于 app/（L20）。
//
// 周期任务采用**固定间隔、不补偿追赶**：每次触发后从当次时刻重排下一次。
// 长时间卡顿（例如调试器断住）之后不会一次补跑几十次——那会让界面雪崩。
#ifndef TERMCHART_UTIL_SCHEDULER_HPP
#define TERMCHART_UTIL_SCHEDULER_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "termchart/export.hpp"
#include "termchart/util/timer.hpp"

namespace termchart::util {

class Scheduler {
public:
    using TaskId = std::uint64_t;
    using Callback = std::function<void()>;

    // 一次性任务：delay 之后触发一次。
    [[nodiscard]] TERMCHART_API TaskId after(Duration delay, Callback callback);
    // 周期任务：每 interval 触发一次（interval <= 0 视为每 tick 一次）。
    [[nodiscard]] TERMCHART_API TaskId every(Duration interval, Callback callback);

    // 显式基准版：以给定时刻算到期，**必须与 tick(stamp) 用同一个时间源**。
    // 测试与自定义时钟走这两个；上面两个便捷版内部用 now()，只该与 tick(now()) 配用。
    // （混用两个时间源会让到期时刻算错——这是本类曾经的设计缺陷，故在此显式区分。）
    [[nodiscard]] TERMCHART_API TaskId after_at(TimePoint stamp, Duration delay,
                                                Callback callback);
    [[nodiscard]] TERMCHART_API TaskId every_at(TimePoint stamp, Duration interval,
                                                Callback callback);

    // 取消任务；任务不存在（已触发过的一次性任务）返回 false。
    TERMCHART_API bool cancel(TaskId id);
    [[nodiscard]] TERMCHART_API bool contains(TaskId id) const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t pending() const noexcept;
    // 最近一个到期时刻；没有任务时返回 nullopt。
    [[nodiscard]] TERMCHART_API std::optional<TimePoint> next_due() const;

    // 执行所有"到期时刻 <= stamp"的任务，返回执行次数（周期任务记一次）。
    // 回调中可以安全地登记新任务或取消任务：
    //   * 到期集合在开始前快照，但每次调用前会按 id 重新查表——被取消的不执行；
    //   * 本轮新登记的任务即使也到期，也留到下一次 tick（本轮不递归）。
    TERMCHART_API std::size_t tick(TimePoint stamp);

private:
    struct Task {
        TaskId id = 0;
        TimePoint due{};
        Duration interval{};
        bool periodic = false;
        Callback callback;
    };

    [[nodiscard]] std::vector<Task>::iterator find(TaskId id) noexcept;

    std::vector<Task> tasks_;
    TaskId next_id_ = 1;
};

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_SCHEDULER_HPP
