// seasonal.hpp —— 季节分解（趋势 + 季节 + 余项）
//
// 字符化策略：内区纵向三分带（trend/season/remainder），自上而下各画一条
// 曲线 '•'，带名写在左侧留白区。统计口径：period = m（≥2，不足两个周期不
// 绘制）；trend 为窗口 m 的居中移动平均（边缘不足半窗为 NaN）；season[j]
// = 按 i mod m 相位平均去趋势残差；remainder = x - trend - season[相位]。
#ifndef TERMCHART_CHART_STATISTICAL_SEASONAL_HPP
#define TERMCHART_CHART_STATISTICAL_SEASONAL_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

struct SeasonalOptions {
    int period = 12;  // 周期长度 m，内部 clamp 到 ≥2
};

class SeasonalChart final : public Chart {
public:
    TERMCHART_API void set_values(std::vector<double> values,
                                  Color color = default_color(DefaultColor::Neutral));

    void set_options(SeasonalOptions options) { options_ = options; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 三个分量与输入等长（未定义处为 NaN；未分解时为空）。
    [[nodiscard]] const std::vector<double>& trend() const noexcept { return trend_; }
    [[nodiscard]] const std::vector<double>& seasonal() const noexcept { return seasonal_; }
    [[nodiscard]] const std::vector<double>& remainder() const noexcept { return remainder_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    void decompose();

    std::vector<double> values_;
    Color color_ = default_color(DefaultColor::Neutral);
    SeasonalOptions options_{};
    std::vector<double> trend_;
    std::vector<double> seasonal_;
    std::vector<double> remainder_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_STATISTICAL_SEASONAL_HPP
