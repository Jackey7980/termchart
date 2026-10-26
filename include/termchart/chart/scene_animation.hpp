// scene_animation.hpp —— 动画**后处理拦截器**：按进度改写一份已经生成的 Scene
//
// 两代做法，这一代是后者：
//   1. 源端（第一代）：`LineChart::to_scene` 里读 `progress_of`，边生成边截断。
//      缺点是"动画"散在每个图种的绘制代码里——加一个图种就要重写一遍，且
//      Scene 无法缓存（同一份数据每个进度都要重新生成整张图，轴/刻度/文本全重算）。
//   2. 后处理（本文件）：`to_scene` 只出**完整几何**，进度由这里统一施加。
//      一份基准 Scene 可以反复生成多帧，动画逻辑只有一个实现点。
//
// 纯几何改写，Scene 里没有数据语义，所以只认两条通用不变式：
//   * **组内才是数据**：轴/网格/标题/图例都在 GroupOp 之外，天然不动；
//   * **柱的基线是组内共享的那条边**：垂直柱从它长出去。
//
// 代价也要写明：截断发生在**屏幕空间**（Scene 只有格坐标），非线性比例尺下
// "等比例揭开"与在数据坐标插值的结果会差一点点。视觉上是擦除，不是重投影，
// 对入场动画足够；要严格的按数据插值，仍可用源端那条路（draw）。
#ifndef TERMCHART_CHART_SCENE_ANIMATION_HPP
#define TERMCHART_CHART_SCENE_ANIMATION_HPP

#include <cstdint>
#include <span>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::chart {

// 三个开关只为一件事：**不同图种的数据图元种类不同**。
// 默认全开；遇到"组里还有不参与动画的矩形"（例如背景框）时按需关掉。
struct SceneAnimationOptions {
    bool clip_lines = true;  // 组内路径按 X 截断（折线从左向右画出）
    bool grow_bars = true;   // 组内矩形从基线向外长出（柱从底部长高）
    bool clip_texts = true;  // 组内文本（数据点标记 ●）跟着截断一起消失
};

// 所有数据组共用同一个进度。最简单的一条路，也是 `apply_animation(scene, p)`
// 的字面语义；多系列各自进度用下面的重载。
//
// 进度 >= 1：**原样返回**（不做任何重建），保证"动画结束 == 没有动画"逐位成立。
// 进度 <= 0：组内几何全部丢弃（组标记保留，交互层拿到的仍是一张合法 Scene）。
TERMCHART_API void apply_animation(scene::Scene& scene, double progress,
                                   const SceneAnimationOptions& options = {});

// 按组进度：`Chart::animation_groups()` 的产出直接喂进来即可。
// 表里没有的组按 1.0（完全显示）处理——与 `Chart::progress_of` 口径一致。
TERMCHART_API void apply_animation(scene::Scene& scene,
                                   std::span<const GroupProgress> groups,
                                   const SceneAnimationOptions& options = {});

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_SCENE_ANIMATION_HPP
