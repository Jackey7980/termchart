// chord.hpp —— 和弦图（实体环形排布 + 弧带连接，值定带宽）
//
// 字符化策略：实体沿椭圆环等分排布（横向半径按字符格 1:2 宽高比放大为纵向的
// 两倍，环本身复用 graph/shape 的 Ellipse 轮廓——格粒度台阶不做假精度）；弦
// 画成弯向圆心的二次贝塞尔弧，逐格按走向取 ─ │ ╱ ╲；连接值越大带越宽，做法
// 是把锚点沿环的切向展开成若干条平行弧。
#ifndef TERMCHART_GRAPH_DIAGRAMS_CHORD_HPP
#define TERMCHART_GRAPH_DIAGRAMS_CHORD_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct ChordEntity {
    std::string label;
};

// 一条弦：from/to 是 entities 的下标；value 决定带宽（<=0 的弦跳过不画，
// from == to 的自弦在环上没有意义，同样跳过）。
struct ChordLink {
    std::size_t from = 0;
    std::size_t to = 0;
    double value = 1.0;
};

struct ChordOptions {
    double radius = 8.0;   // 环的纵向半径（格）；横向半径 = 2 * radius 补偿宽高比
    int max_band = 3;      // 最大带宽（平行弧条数，>=1）
    Style ring_style{};
    Style band_style{};
    Style label_style{};
    bool draw_ring = true;
};

// 就地绘制；返回环与标签的包围盒（实体为空返回零矩形）。
// 越界部分由 canvas 裁剪；下标越界的弦跳过。
[[nodiscard]] TERMCHART_API Rect render_chord(const std::vector<ChordEntity>& entities,
                                              const std::vector<ChordLink>& links,
                                              canvas::Canvas& canvas,
                                              const ChordOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_CHORD_HPP
