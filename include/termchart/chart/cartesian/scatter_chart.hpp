// scatter_chart.hpp —— 散点图（可选气泡权重）
//
// 与折线图的唯一区别：**不连线**，每个点独立标记。点可以带"权重"（气泡图）：
// 权重被归一化后映射到四档标记字符（· ○ ● ◉），而不是浮点半径 —— 字符网格上
// 画不出真正的半径，硬做只会得到看起来随机的大小。
#ifndef TERMCHART_CHART_CARTESIAN_SCATTER_CHART_HPP
#define TERMCHART_CHART_CARTESIAN_SCATTER_CHART_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

class ScatterChart final : public Chart {
public:
    TERMCHART_API void add_series(Series series);
    // 气泡系列：weights 与 series.points 按下标对应，缺的按 0 处理。
    TERMCHART_API void add_bubble_series(Series series, std::vector<double> weights);

    void set_auto_domain(bool on, bool include_zero = false) {
        auto_domain_ = on;
        include_zero_ = include_zero;
    }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Series> series_;
    std::vector<std::vector<double>> weights_;  // 与 series_ 一一对应；空表示非气泡
    bool auto_domain_ = true;
    bool include_zero_ = false;
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_SCATTER_CHART_HPP
