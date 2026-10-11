// nice.hpp —— nice number 算法（刻度与值域对齐的核心）
//
// 「好看的数」= 1 / 2 / 5 × 10^k。它让轴标签对人类可读，而不是 0.317、3.142…
// 这是 Heckbert「Graphics Gems」经典算法的最小可用版，被 ticks/auto_domain 复用。
#ifndef TERMCHART_SCALE_NICE_HPP
#define TERMCHART_SCALE_NICE_HPP

#include <cmath>

#include "termchart/export.hpp"
#include "termchart/util/math.hpp"

namespace termchart::scale {

// 把正数 x 收成最近的 nice 数。round=true 时向上取整（用于步长），
// round=false 时就近（用于值域对齐）。
TERMCHART_API double nice_number(double x, bool round = false) noexcept;

// 给定跨度与目标刻度个数，返回 nice 步长（span / target 的 nice 上界）。
TERMCHART_API double nice_step(double span, int target_ticks) noexcept;

}  // namespace termchart::scale

#endif  // TERMCHART_SCALE_NICE_HPP
