// waterfall.hpp —— 瀑布图：顺序增量/减量 + 小计
//
// 终端字符化策略：每段是**从累计基线画到新累计值**的 █ 柱（增/减/小计三色
// 区分语义，这是瀑布图区别于普通柱状图的关键）；相邻柱之间在上一段结束的
// 累计高度画 ─ 连接线，视觉上串起"流水"。x 轴是条目序号。
#ifndef TERMCHART_CHART_CARTESIAN_WATERFALL_HPP
#define TERMCHART_CHART_CARTESIAN_WATERFALL_HPP

#include <string>
#include <utility>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

class WaterfallChart final : public Chart {
public:
    struct Item {
        std::string label;
        double value = 0.0;
        bool is_total = false;  // true：累计重置为 value（基线回到 0）
    };
    struct Options {
        Color increase = default_color(DefaultColor::Accent);
        Color decrease = default_color(DefaultColor::Warn);
        Color total = default_color(DefaultColor::Neutral);
        bool show_connectors = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<Item> items);

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }
    // set_data 后的累计序列（与保留的条目一一对应，小计项为其自身值）。
    [[nodiscard]] const std::vector<double>& cumulative() const noexcept {
        return cumulative_;
    }

private:
    Options opts_{};
    std::vector<Item> items_;
    std::vector<double> cumulative_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_WATERFALL_HPP
