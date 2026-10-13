// yield_curve.hpp —— 收益率曲线（到期期限 → 收益率）
//
// 字符化策略：点间以 '•' 连线、每个观测点落 '●'；x = 到期期限（绘制前按
// 升序重排，重复期限取首见），y = 收益率。非有限观测剔除。
#ifndef TERMCHART_CHART_FINANCIAL_YIELD_CURVE_HPP
#define TERMCHART_CHART_FINANCIAL_YIELD_CURVE_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

struct YieldCurveOptions {
    bool show_markers = true;
};

class YieldCurveChart final : public Chart {
public:
    TERMCHART_API void set_curve(std::vector<double> maturities,
                                 std::vector<double> yields,
                                 Color color = default_color(DefaultColor::Cyan),
                                 std::string name = "yield");

    void set_options(YieldCurveOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const std::vector<Point>& points() const noexcept { return points_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Point> points_;  // (期限, 收益率)，按期限升序
    Color color_ = default_color(DefaultColor::Cyan);
    std::string name_ = "yield";
    YieldCurveOptions options_{};
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_FINANCIAL_YIELD_CURVE_HPP
