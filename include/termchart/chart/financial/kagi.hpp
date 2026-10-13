// kagi.hpp —— 卡吉图（转折摆动列，粗细表强弱）
//
// 字符化策略：按反转阈值抽出 zigzag 摆动，每个摆动占一列：竖段用
// ┃（阳：超过前一同向极值）/ │（阴），转折位用 ─ 连到下一列。
// reversal<=0 时自动取全距 5%。
#ifndef TERMCHART_CHART_FINANCIAL_KAGI_HPP
#define TERMCHART_CHART_FINANCIAL_KAGI_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

struct KagiOptions {
    double reversal = 0.0;  // <=0 → 自动 = 全距 * 5%
    Color yang = default_color(DefaultColor::Accent);
    Color yin = default_color(DefaultColor::Warn);
};

class KagiChart final : public Chart {
public:
    void set_prices(std::vector<double> prices);

    void draw(DrawContext ctx) override;

    KagiOptions options;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> prices_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_FINANCIAL_KAGI_HPP
