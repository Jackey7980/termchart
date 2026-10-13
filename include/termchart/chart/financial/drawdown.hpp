// drawdown.hpp —— 回撤图（水下曲线）
//
// 字符化策略：回撤线 '•' 逐点落格，线与 0 基线之间铺 '░'（水下填充）。
// 统计口径：dd_i = p_i / peak_i - 1（peak_i 为前缀最大价），恒 ≤ 0；y 域
// [min dd, 0]。非有限价格剔除，剩余不足 1 个不绘制。
#ifndef TERMCHART_CHART_FINANCIAL_DRAWDOWN_HPP
#define TERMCHART_CHART_FINANCIAL_DRAWDOWN_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

struct DrawdownOptions {
    Color color = default_color(DefaultColor::Warn);  // 水下红
    bool fill = true;                // 0 基线以下铺 '░'
};

class DrawdownChart final : public Chart {
public:
    void set_prices(std::vector<double> prices,
                    Color color = default_color(DefaultColor::Warn));

    void set_options(DrawdownOptions options) { options_ = options; }

    void draw(DrawContext ctx) override;

    // 回撤序列（与有效样本一一对应，恒 ≤ 0；无有效样本时为空）。
    [[nodiscard]] const std::vector<double>& drawdown() const noexcept { return dd_; }
    [[nodiscard]] double max_drawdown() const noexcept;
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> dd_;
    Color color_ = default_color(DefaultColor::Warn);
    DrawdownOptions options_{};
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_FINANCIAL_DRAWDOWN_HPP
