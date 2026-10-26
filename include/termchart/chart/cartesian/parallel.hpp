// parallel.hpp —— 平行坐标：每个维度一条竖轴均分画布宽，每条观测跨轴连成折线。
//
// 终端字符化策略：各维独立 min-max 归一化到竖轴后逐段画 • 折线（颜色按观测轮换），
// 高维相关模式在二维终端上靠"线的走向"直接可读。
#ifndef TERMCHART_CHART_CARTESIAN_PARALLEL_HPP
#define TERMCHART_CHART_CARTESIAN_PARALLEL_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

class ParallelChart final : public Chart {
public:
    struct Options {
        bool show_axes = true;     // 每维竖轴 + 顶部维度名
        bool show_markers = false; // 观测折线的顶点是否加 ●
    };

    TERMCHART_API void set_axes(std::vector<std::string> names,
                                std::vector<std::vector<double>> observations);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 真实 Scene 输出：各维竖轴 + 跨轴样本折线用矢量 StrokeOp 复刻，维名走 TextOp。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<std::string> names_;
    std::vector<std::vector<double>> observations_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_PARALLEL_HPP
