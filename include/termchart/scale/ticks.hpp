// ticks.hpp —— 刻度生成
//
// 两种入口：
//   * nice_ticks：纯数据轴，无需比例尺，直接给 [dmin,dmax] 上的 nice 刻度；
//   * ticks_for：已有 Scale（线性/对数都行），在其 range 上均匀取点，但刻度
//     值对应 nice 的数据坐标——所以对数轴上刻度是 1/10/100 而非等距空格。
#ifndef TERMCHART_SCALE_TICKS_HPP
#define TERMCHART_SCALE_TICKS_HPP

#include <vector>

#include "termchart/export.hpp"
#include "termchart/scale/scale.hpp"

namespace termchart::scale {

// 落在 [dmin,dmax]（自动归一顺序）内的 nice 刻度值，按升序返回。
TERMCHART_API std::vector<double> nice_ticks(double domain_min, double domain_max,
                                             int target = 5) noexcept;

// 对 Scale 生成刻度：在 range 上均匀取约 target 个点（用 nice_step 定步长），
// 再 inverse 回数据坐标。要求 scale.valid()。
TERMCHART_API std::vector<double> ticks_for(const Scale& scale, int target = 5) noexcept;

}  // namespace termchart::scale

#endif  // TERMCHART_SCALE_TICKS_HPP
