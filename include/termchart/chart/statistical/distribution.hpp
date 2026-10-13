// distribution.hpp —— 分布曲线（高斯核密度估计 KDE）
//
// 字符化策略：KDE 曲线逐点落 '•'，曲线下方按列铺浅色 '▒'；统计口径：带宽
// h = 0.9·min(样本标准差, IQR/1.34)·n^(-1/5)（Silverman 经验法则，退化时取
// 单位带宽），曲线在 [min-3h, max+3h] 上等距评估 grid_points 个点，y 为
// 归一化密度。非有限样本剔除。
#ifndef TERMCHART_CHART_STATISTICAL_DISTRIBUTION_HPP
#define TERMCHART_CHART_STATISTICAL_DISTRIBUTION_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

struct DistributionOptions {
    int grid_points = 96;    // KDE 评估点数，内部 clamp 到 [16, 512]
    double bandwidth = 0.0;  // <=0：自动（Silverman）
    bool fill_under = true;  // 曲线下方铺 '▒'
};

class DistributionChart final : public Chart {
public:
    TERMCHART_API void set_values(std::vector<double> values,
                                  Color color = default_color(DefaultColor::Accent),
                                  std::string name = "density");

    void set_options(DistributionOptions options) {
        options_ = options;
        evaluate();
    }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }
    // 最近一次评估的 KDE 曲线（x 升序；无有效样本时为空）。
    [[nodiscard]] const std::vector<double>& curve_x() const noexcept { return xs_; }
    [[nodiscard]] const std::vector<double>& curve_y() const noexcept { return ys_; }

private:
    // 由 set_values/set_options 触发的确定性重估。
    void evaluate();

    std::vector<double> values_;
    Color color_ = default_color(DefaultColor::Accent);
    std::string name_ = "density";
    DistributionOptions options_{};
    bool show_legend_ = true;
    std::vector<double> xs_;
    std::vector<double> ys_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_STATISTICAL_DISTRIBUTION_HPP
