// bar_chart.hpp —— 笛卡尔柱状图（单系列，垂直柱）
//
// 与 LineChart 同构：网格 → 轴 → 从基线起按值填充竖条（█）→ 图例 → 标题。
// 分组/堆叠在多系列场景才需要，作为后续扩展（见 ROADMAP）。
#ifndef TERMCHART_CHART_CARTESIAN_BAR_CHART_HPP
#define TERMCHART_CHART_CARTESIAN_BAR_CHART_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/scale/auto_domain.hpp"

namespace termchart::chart {

class BarChart final : public Chart {
public:
    // 柱用单系列即可；values 为每个类别的值，categories 为对应名称。
    void set_values(std::vector<double> values, std::vector<std::string> categories,
                    Color color = Color::ansi16(2), std::string name = "series");
    void set_show_legend(bool on) { show_legend_ = on; }

    void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> values_;
    std::vector<std::string> categories_;
    Color color_ = Color::ansi16(2);
    std::string name_ = "series";
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_BAR_CHART_HPP
