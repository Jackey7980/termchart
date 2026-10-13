// line_chart.hpp —— 笛卡尔折线图
//
// 把若干 Series 画进 Canvas：网格 → 坐标轴+刻度 → 折线（含数据点标记）→
// 图例 → 标题。域可自动按数据范围 + 5% 留白 + nice 对齐（scale::nice_domain）。
#ifndef TERMCHART_CHART_CARTESIAN_LINE_CHART_HPP
#define TERMCHART_CHART_CARTESIAN_LINE_CHART_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/scale/auto_domain.hpp"

namespace termchart::chart {

class LineChart final : public Chart {
public:
    // 追加一条系列（append 语义）。
    void add_series(Series s) { series_.push_back(std::move(s)); }
    // 替换全部系列（replace 语义）。**两个动词成对提供**：全库约定 `add_*` 是追加、
    // `set_*` 是替换，只给一半会逼调用方自己 clear()，而 clear() 并不总是对外可见。
    void set_series(std::vector<Series> series) { series_ = std::move(series); }
    [[nodiscard]] const std::vector<Series>& series() const noexcept { return series_; }

    void set_auto_domain(bool on, bool include_zero = false) {
        auto_domain_ = on;
        include_zero_ = include_zero;
    }
    void set_show_legend(bool on) { show_legend_ = on; }

    void draw(DrawContext ctx) override;

    // 暴露最后使用的绘图区，供测试断言几何。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Series> series_;
    bool auto_domain_ = true;
    bool include_zero_ = false;
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_LINE_CHART_HPP
