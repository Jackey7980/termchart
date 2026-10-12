// animation.hpp —— 动画与缓动
//
// 口径（全模块统一，调用方不需要再猜）：
//   * 输入 t 与输出都在 [0,1]；ease() 先把 t 钳位到 [0,1] 再过缓动曲线；
//     非有限 t（NaN/Inf）不可比较，一律视为界外 → 0；
//   * 时间单位是**秒**（double），与 util/timer.hpp 一致；
//   * 时钟可注入（AnimationClock）：单测与 headless 不依赖真实时间流逝，
//     真实时间只是缺省实现（steady_clock，单调，见 util/timer.hpp 的理由）。
#ifndef TERMCHART_APP_ANIMATION_HPP
#define TERMCHART_APP_ANIMATION_HPP

#include <cstdint>
#include <functional>

#include "termchart/export.hpp"

namespace termchart::app {

// 缓动曲线族。选择原则：终端动画大多是 UI 过渡（面板展开、数值滚动），
// 二次/三次曲线族够用且单调；不带 overshoot 的 Back/elastic——回弹会让
// 字符画布上的元素"越过"目标格，与"输出在 [0,1]"的口径冲突。
enum class Easing : std::uint8_t {
    Linear,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
};

// 缓动映射：t 会被钳位；输出在 [0,1]。
[[nodiscard]] TERMCHART_API double ease(Easing easing, double t) noexcept;

// 时钟：返回"当前时刻"（秒）。缺省实现基于 steady_clock。
using AnimationClock = std::function<double()>;

// 单段动画：时长 + 缓动 + 起点。时间全部可注入（value_at / finished_at
// 是纯函数，真实时钟版本只是包一层）。
class Animation {
public:
    // duration_seconds <= 0 视为 0：start 后立即 finished、value 为 1。
    explicit Animation(double duration_seconds, Easing easing = Easing::EaseInOutQuad,
                       AnimationClock clock = AnimationClock{});

    // 以当前时钟为起点。
    void start() noexcept;

    // 以指定时刻为起点（测试 / 与外部时钟对齐用）。
    void start_at(double now_seconds) noexcept;

    // 当前进度（缓动后，[0,1]）。未 start → 0。
    [[nodiscard]] double value() noexcept;

    // 纯函数版本：不读时钟。
    [[nodiscard]] double value_at(double now_seconds) const noexcept;

    [[nodiscard]] bool finished() noexcept;
    [[nodiscard]] bool finished_at(double now_seconds) const noexcept;

    [[nodiscard]] double duration() const noexcept { return duration_; }
    [[nodiscard]] Easing easing() const noexcept { return easing_; }

private:
    [[nodiscard]] double now() const noexcept;

    double duration_;
    Easing easing_;
    AnimationClock clock_;
    double start_time_ = 0.0;
    bool started_ = false;
};

}  // namespace termchart::app

#endif  // TERMCHART_APP_ANIMATION_HPP
