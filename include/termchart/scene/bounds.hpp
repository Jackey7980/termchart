// bounds.hpp —— Scene 的几何包围盒
//
// 为什么需要它：GUI 侧的 `render/scene_renderer` 要先知道"这份 Scene 有多大"才能
// 决定画布/无效区；终端侧也可以用它预分配画布。放在 scene/（L2）而不是 canvas/，
// 因为消费 Scene 的**每个**前端都需要它——不只终端那一条路径。
//
// 计算口径（刻意保守，宁可略大也不要漏）：
//   * 线段取两端点；
//   * 圆弧取"圆心 ± 半径"的正方形（不做最小包围盒）；
//   * 文本按**显示宽度** × 1 行估算（宽字符算 2 列）；
//   * 图片取它的矩形；
//   * **不计描边宽度**：线宽会让实际墨迹略微外扩，本函数只保证几何上包得住。
#ifndef TERMCHART_SCENE_BOUNDS_HPP
#define TERMCHART_SCENE_BOUNDS_HPP

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::scene {

// 空 Scene 返回零矩形（w == h == 0）。
[[nodiscard]] TERMCHART_API Rect scene_bounds(const Scene& scene) noexcept;

}  // namespace termchart::scene

#endif  // TERMCHART_SCENE_BOUNDS_HPP
