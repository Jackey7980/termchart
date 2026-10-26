// qq.hpp —— QQ 图：样本分位数对理论正态分位数逐点落格，附参考线判断正态性。
//
// 终端字符化策略：x 轴为理论正态分位数（Acklam 逆逼近），y 轴为排序样本值，
// 点用 ●，参考线 y=μ+σx 用 •；点贴线即正态，弯曲形态即偏差方向。
#ifndef TERMCHART_CHART_CARTESIAN_QQ_HPP
#define TERMCHART_CHART_CARTESIAN_QQ_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

class QQChart final : public Chart {
public:
    struct Options {
        bool show_line = true;  // y = μ + σ·x 参考线
        Color point_color = default_color(DefaultColor::Neutral);
    };

    TERMCHART_API void set_sample(std::vector<double> sample);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 真实 Scene 输出：参考线走 LineOp，样本点走 TextOp（● 字形）。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> sample_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_QQ_HPP
