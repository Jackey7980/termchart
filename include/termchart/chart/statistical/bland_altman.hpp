// bland_altman.hpp —— 一致性图（Bland-Altman）
//
// 字符化策略：每对方法画一点 '●'，横轴为两法均值、纵轴为差值；mean 差画
// '─' 实线，mean ± 1.96·SD 一致性界限画 '·' 虚线。统计口径：对有限且成对
// 的样本计算 d_i = a_i - b_i、m_i = (a_i + b_i)/2，mean = avg(d)、
// SD = 样本标准差（除以 n-1）；有效对 < 2 不绘制。
#ifndef TERMCHART_CHART_STATISTICAL_BLAND_ALTMAN_HPP
#define TERMCHART_CHART_STATISTICAL_BLAND_ALTMAN_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

struct BlandAltmanOptions {
    Color color = default_color(DefaultColor::Neutral);
    bool show_limits = true;  // ±1.96·SD 界限
};

class BlandAltmanChart final : public Chart {
public:
    TERMCHART_API void set_pairs(std::vector<double> method_a,
                                 std::vector<double> method_b,
                                 Color color = default_color(DefaultColor::Neutral),
                                 std::string name = "diff");

    void set_options(BlandAltmanOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] double mean_diff() const noexcept { return mean_; }
    [[nodiscard]] double sd_diff() const noexcept { return sd_; }
    [[nodiscard]] double upper_limit() const noexcept { return mean_ + 1.96 * sd_; }
    [[nodiscard]] double lower_limit() const noexcept { return mean_ - 1.96 * sd_; }
    [[nodiscard]] const std::vector<Point>& pairs() const noexcept { return pairs_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    BlandAltmanOptions options_{};
    bool show_legend_ = true;
    std::string name_ = "diff";
    std::vector<Point> pairs_;  // (均值, 差值)
    bool valid_ = false;
    double mean_ = 0.0;
    double sd_ = 0.0;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_STATISTICAL_BLAND_ALTMAN_HPP
