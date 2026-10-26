// easing.hpp —— 缓动曲线族（纯数学，无状态）
//
// 为什么在 util 而不是 app：缓动只依赖 `util::math`，没有任何时间/IO 语义；
// 而消费方 `chart`(L12) 在 `app`(L20) 底下，按分层规则不能反向依赖 app。
// 曲线族的唯一真源放在这里，`app::Easing` / `app::ease` 作为别名转发，
// 老的调用点（`app/animation.hpp`、`examples/25`）零改动。
//
// 口径（与 app::Animation 一致）：
//   * 输入 t 与输出都在 [0,1]；ease() 先把 t 钳位到 [0,1] 再过曲线；
//     非有限 t（NaN/Inf）不可比较，一律视为界外 → 0。
#ifndef TERMCHART_UTIL_EASING_HPP
#define TERMCHART_UTIL_EASING_HPP

#include <cstdint>

#include "termchart/export.hpp"

namespace termchart::util {

// 缓动曲线族。选择原则：终端动画大多是 UI 过渡（面板展开、数值滚动），
// 二次/三次曲线族够用且单调；不带 overshoot 的 Back/Elastic——回弹会让
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

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_EASING_HPP
