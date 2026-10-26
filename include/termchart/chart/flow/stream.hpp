// stream.hpp —— 流向图（ThemeRiver 语义）
//
// 规格里本文件是"转发 cartesian/streamgraph"的同层语义包装；但
// cartesian/streamgraph 属另一工作包且尚未落地，为不让本包空转，
// 这里先自给堆叠算法（各列按占比从基线向另一侧堆叠 █），待
// cartesian/streamgraph 落地后可改回纯转发。布局不依赖 graph/。
#ifndef TERMCHART_CHART_FLOW_STREAM_HPP
#define TERMCHART_CHART_FLOW_STREAM_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/chart/series.hpp"

namespace termchart::chart {

struct StreamOptions {
    bool centered = true;  // true: ThemeRiver 中心对称；false: 自底向上堆叠
};

class StreamChart final : public Chart {
public:
    // 各系列点数必须一致（按 y 值逐列堆叠，x 仅作对齐索引）。
    void set_series(std::vector<Series> series);

    void draw(DrawContext ctx) override;

    // 真实 Scene 输出：手算 inner 矩形（无坐标轴），逐列占比堆叠走 RectOp +
    // FillOp，图例/标题走 TextOp。与 draw() 同源。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    StreamOptions options;

private:
    std::vector<Series> series_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_FLOW_STREAM_HPP
