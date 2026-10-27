// animation.hpp —— 动画的两块基本件：**配置**与**运行时状态**
//
// 为什么要把它们从 chart.hpp 里拆出来：
//   * `AnimationTrack` 只是"多久、多晚、怎么缓"三个数，与 Chart 无关；
//   * `AnimationState` 是这些数 + 一个累加的 elapsed，可以脱离 Chart 单独用
//     （GUI 帧循环、导出脚本里自己算进度都行）；
//   * Chart 只负责"按名字表持有这些状态"，不再自己拿着时钟做减法。
//
// 两套时钟口径并存且**共用同一份实现**：
//   * `Chart::tick(now)`  绝对时刻（首次 tick 即起点）——暂停/续播/跳转友好；
//   * `AnimationState::tick(dt)` 增量步进——帧循环天然拿到的是 dt。
// Chart 内部把 `now` 差分成 dt 再喂给状态，于是两条路算出的进度逐位一致。
#ifndef TERMCHART_CHART_ANIMATION_HPP
#define TERMCHART_CHART_ANIMATION_HPP

#include "termchart/export.hpp"
#include "termchart/util/easing.hpp"

namespace termchart::chart {

using util::ease;
using util::Easing;

// 一段按名动画：延迟 + 时长 + 缓动曲线。
// "名字"就是 `Series::name`（也就是图例上那行文本），用户不必知道任何内部 id。
struct AnimationTrack {
    double duration = 0.5;  // 秒；<= 0 或非有限 → 瞬间完成
    double delay = 0.0;     // 秒；负数按 0 处理
    Easing easing = Easing::EaseOutCubic;
};

// 进度状态机。**未 start 时 progress() 恒为 1.0**——这是刻意的安全阀：
// 静态渲染路径（从不 tick）不该因为"登记过动画"而变成空白图。
//
// 寿命：`reset()` 回到未启动（下次 tick 重新计时）；`restart()` 语义同 reset，
// 保留两个名字是因为调用点读起来不一样。
class AnimationState {
public:
    AnimationState() = default;
    explicit AnimationState(AnimationTrack cfg) noexcept : track(cfg) {}

    // 配置（公开可改：改时长不影响已经累计的 elapsed，与"改参数立即生效"一致）。
    AnimationTrack track;

    // 增量推进。dt <= 0 或非有限一律忽略（脏时钟不污染时间轴）。
    // 第一次**有效**调用把状态从"未启动"切成"运行中"，elapsed 从 0 开始。
    TERMCHART_API void tick(double dt) noexcept;

    TERMCHART_API void reset() noexcept;

    // [0,1]。未启动 → 1.0；延迟内 → 0.0；零时长 → 延迟一过即 1.0。
    [[nodiscard]] TERMCHART_API double progress() const noexcept;

    // 已启动且 elapsed 越过 delay+duration。
    [[nodiscard]] TERMCHART_API bool finished() const noexcept;
    [[nodiscard]] bool started() const noexcept { return started_; }
    [[nodiscard]] double elapsed() const noexcept { return elapsed_; }

private:
    double elapsed_ = 0.0;  // 含延迟的累计时间
    bool started_ = false;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_ANIMATION_HPP
