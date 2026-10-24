// line_chart.hpp —— 笛卡尔折线图
//
// 把若干 Series 画进 Canvas：网格 → 坐标轴+刻度 → 折线（含数据点标记）→
// 图例 → 标题。域可自动按数据范围 + 5% 留白 + nice 对齐（scale::nice_domain）。
#ifndef TERMCHART_CHART_CARTESIAN_LINE_CHART_HPP
#define TERMCHART_CHART_CARTESIAN_LINE_CHART_HPP

#include <functional>
#include <string>
#include <utility>
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

    // x 轴刻度标签格式化钩子：**只影响标签文本，不影响刻度位置**（位置仍由
    // scale::ticks_for 的 nice 值决定）。给时间轴用——x 是 epoch 秒这类连续
    // 数值、但人要看 "03-01" 而不是 "1772350000" 时，由调用方注入格式化，
    // chart 层不认识时间语义（与 layout 的字符串标签入口分层一致）。
    // 未设置时走 layout::format_tick 的默认数字格式。
    void set_x_tick_formatter(std::function<std::string(double)> formatter) {
        x_tick_formatter_ = std::move(formatter);
    }

    void draw(DrawContext ctx) override;

    // 暴露最后使用的绘图区，供测试断言几何。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Series> series_;
    bool auto_domain_ = true;
    bool include_zero_ = false;
    bool show_legend_ = true;
    std::function<std::string(double)> x_tick_formatter_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_LINE_CHART_HPP
