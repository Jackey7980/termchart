// point_figure.hpp —— 点数图（Point & Figure）
//
// 字符化策略：价格按 box_size 量化为格档，X 列（上行情）落 'X'、O 列（下行
// 情落 'O'，每档一格、沿列纵向堆叠；方向反转需回调 reversal 格（默认 3）。
// 统计口径：box = floor(p/box_size)；上行列只抬顶、下行列只沉底；box_size
// ≤ 0 时取全距/10，全距为 0 或价格不足 2 个不绘制。列的 top/bottom 以格档
// 号给出（top ≥ bottom，单档列为 top == bottom）。
#ifndef TERMCHART_CHART_FINANCIAL_POINT_FIGURE_HPP
#define TERMCHART_CHART_FINANCIAL_POINT_FIGURE_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

struct PointFigureOptions {
    double box_size = 0.0;  // ≤0 → 自动 = 全距/10
    int reversal = 3;       // 反转所需格数（≥1）
    Color up = default_color(DefaultColor::Accent);
    Color down = default_color(DefaultColor::Warn);
};

struct PfColumn {
    bool up = true;
    int top = 0;     // 最高格档号
    int bottom = 0;  // 最低格档号
};

class PointFigureChart final : public Chart {
public:
    void set_prices(std::vector<double> prices);

    void draw(DrawContext ctx) override;

    PointFigureOptions options;

    [[nodiscard]] const std::vector<PfColumn>& columns() const noexcept { return columns_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> prices_;
    std::vector<PfColumn> columns_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_FINANCIAL_POINT_FIGURE_HPP
