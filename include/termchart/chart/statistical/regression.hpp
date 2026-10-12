// regression.hpp —— 线性回归 + 残差
//
// 字符化策略：样本落 '●'、拟合线 '─'、残差竖线 '·'（点到拟合线的竖直偏差）。
// 统计口径：普通最小二乘 y = a + b·x，只对有限样本拟合，n ≥ 2 且 x 方差 > 0
// 才成立；R² = 1 - SSres/SStot。拟合失败时 slope/intercept/R² 为 0、
// fitted() 为 false，绘制退化为纯散点。
#ifndef TERMCHART_CHART_STATISTICAL_REGRESSION_HPP
#define TERMCHART_CHART_STATISTICAL_REGRESSION_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

struct RegressionOptions {
    bool show_residuals = true;  // 残差竖线
};

class RegressionChart final : public Chart {
public:
    TERMCHART_API void set_points(std::vector<Point> points,
                                  Color color = Color::ansi16(4),
                                  std::string name = "y");

    void set_options(RegressionOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] bool fitted() const noexcept { return fitted_; }
    [[nodiscard]] double slope() const noexcept { return slope_; }
    [[nodiscard]] double intercept() const noexcept { return intercept_; }
    [[nodiscard]] double r_squared() const noexcept { return r_squared_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Point> points_;
    Color color_ = Color::ansi16(4);
    std::string name_ = "y";
    RegressionOptions options_{};
    bool show_legend_ = true;
    bool fitted_ = false;
    double slope_ = 0.0;
    double intercept_ = 0.0;
    double r_squared_ = 0.0;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_STATISTICAL_REGRESSION_HPP
