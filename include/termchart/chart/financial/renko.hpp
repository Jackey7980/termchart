// renko.hpp —— 砖形图（固定砖值的价格阶梯）
//
// 字符化策略：价格每越过一档 brick_size 生成一块砖，升砖绿、降砖红，
// 每砖 2 格宽 █、沿时间轴逐格右移并上下阶梯。brick_size<=0 时取全距/10。
#ifndef TERMCHART_CHART_FINANCIAL_RENKO_HPP
#define TERMCHART_CHART_FINANCIAL_RENKO_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

struct RenkoOptions {
    double brick_size = 0.0;  // <=0 → 自动 = (max-min)/10
    Color up = Color::ansi16(2);
    Color down = Color::ansi16(1);
};

class RenkoChart final : public Chart {
public:
    void set_prices(std::vector<double> prices);

    void draw(DrawContext ctx) override;

    RenkoOptions options;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> prices_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_FINANCIAL_RENKO_HPP
