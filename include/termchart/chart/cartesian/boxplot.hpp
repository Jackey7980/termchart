// boxplot.hpp —— 箱线图：五数概括 + 须 + 离群点
//
// 终端字符化策略：箱体（q1–q3）画成 █ 实心块，中位线用 ─ 覆盖箱体中行，
// 上下须是 │ 竖线、须端帽是 ─ 短横，离群点用 ○ 单字符。输入直接接受
// 五数概括（min/q1/med/q3/max），不在 chart 层重复实现分位数算法；
// 违反 lo≤q1≤med≤q3≤hi 的组整体剔除。
#ifndef TERMCHART_CHART_CARTESIAN_BOXPLOT_HPP
#define TERMCHART_CHART_CARTESIAN_BOXPLOT_HPP

#include <string>
#include <utility>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

class BoxPlotChart final : public Chart {
public:
    struct BoxStats {
        double lo = 0.0;   // 须下端（通常 min，不含离群点）
        double q1 = 0.0;
        double med = 0.0;
        double q3 = 0.0;
        double hi = 0.0;   // 须上端
        std::vector<double> outliers;  // 须外单点
    };
    struct Options {
        Color color = Color::ansi16(4);
        bool show_outliers = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<BoxStats> groups) { groups_ = std::move(groups); }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<BoxStats> groups_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_BOXPLOT_HPP
