// streamgraph.hpp —— 流图：多层序列围绕居中基线上下堆叠，形成"流动的河"。
//
// 终端字符化策略：逐列从基线向上/向下堆叠各层，用 █ 实心块按层着色；
// 基线可选 silhouette（对称居中）或 wiggle（最小摆动），x 轴是观测序号。
#ifndef TERMCHART_CHART_CARTESIAN_STREAMGRAPH_HPP
#define TERMCHART_CHART_CARTESIAN_STREAMGRAPH_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

class StreamgraphChart final : public Chart {
public:
    enum class Baseline { Silhouette, Wiggle };

    struct Options {
        Baseline baseline = Baseline::Silhouette;
    };

    TERMCHART_API void add_series(Series series);
    // 替换语义（与 add_* 成对提供，见 line_chart.hpp 的同名注释）。
    TERMCHART_API void set_series(std::vector<Series> layers);
    [[nodiscard]] const std::vector<Series>& series() const noexcept { return layers_; }

    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Series> layers_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_STREAMGRAPH_HPP
