// radial.hpp —— 径向树布局（极坐标 + 子树扇区均分）
//
// 思路与经典“径向树（radial tree）”一致：先把有向图按 BFS 摊成一棵根树（与
// tree_layout 同法：入度为 0 的第一个节点为根，全有入边时取第一个节点；只沿出边、
// 首次访问为准，因此有环图也能摆），再给每个节点分配一个角扇区：
//   · 根独占整圆 [start_angle, start_angle + 2π)；
//   · 父节点把自己的扇区**按子节点数均分**给每个孩子（每个孩子再继续细分）。
// 于是同一父下的兄弟角度间隔相等；兄弟的扇区互不相交 → 以兄弟为根的子树占据
// 互不相交的角度范围，子树之间天然不交叉。
//
// 半径只由层号决定：radius(level) = ring_start + level * ring_step。
// 为什么半径按层线性取：径向树的“环”本就是等距同心圆，层号相同 → 半径相同、
// 一眼能看出层次；层号越大 → 离根越远。ring_step 取“节点高 + 间距”量级即可，
// 默认 12：竖直方向经宽高比压缩后为 12/2 = 6 ≥ 节点最小高 4 + 2，相邻环不挤。
//
// 字符格不是正方形（宽 ≈ 高的 2 倍），正圆直接落格会显扁。落格时按
// canvas/transform.hpp::aspect_correction 的同一口径把 y 压缩 1/aspect：
//     gx = cx + r·cos θ，gy = cy + r·sin θ / aspect。
// 因此在格坐标里量到的距离要按同口径还原（dx、dy·aspect）才等于几何半径 r，
// 本布局的“层半径”指的就是这个还原后的真实半径。
//
// 重叠：同一层里角度极近的两个节点，取整落格后可能落在同一格。本布局**允许**
// 这种重叠，但保证**确定**——纯几何、无随机，同输入逐格一致。要减少重叠可把
// min_angle 调大：它给同一父的相邻子扇区之间留出最小角间隙。
#ifndef TERMCHART_GRAPH_LAYOUT_RADIAL_HPP
#define TERMCHART_GRAPH_LAYOUT_RADIAL_HPP

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"

namespace termchart::graph {

struct RadialOptions {
    LayoutOptions layout{};       // 节点尺寸与边距
    double ring_step = 12.0;      // 相邻层半径增量（格）
    double ring_start = 0.0;      // 根的半径；0 表示根落在圆心
    double min_angle = 0.0;       // 同一父相邻子扇区的最小角间隙（弧度）；0 = 纯均分
    double aspect = 2.0;          // 字符格宽高比（宽:高）；口径同 canvas/transform.hpp
    // 根扇区起始角（弧度），默认 -π/2，即第一个子节点朝正上方，顺时针铺开。
    double start_angle = -1.57079632679489661923;
};

// 径向树布局：就地写入 Node::set_box，返回所有节点外框的并集；空图返回零矩形。
// 根取入度为 0 的第一个节点；全部节点都有入边（整图是环）时取第一个节点。
[[nodiscard]] TERMCHART_API Rect layout_radial(Graph& graph,
                                               const RadialOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_LAYOUT_RADIAL_HPP
