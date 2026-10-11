// sparkline.hpp —— 迷你折线（无轴、无网格、无标签）
//
// 用途：表格单元格里的一格趋势、仪表盘上的小趋势线。整块画布就是数据区，
// 因此它**不是** Chart 的子类（没有标题/边距/图例这些概念），只接受一个画布。
//
// 定域口径：默认按数据 min/max 自适应；`set_zero_baseline(true)` 时把 0 纳入域，
// 用于"涨跌是否越过基准线"的读数场景。
#ifndef TERMCHART_CHART_SPARKLINE_HPP
#define TERMCHART_CHART_SPARKLINE_HPP

#include <cstddef>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::chart {

class Sparkline {
public:
    TERMCHART_API void set_values(std::vector<double> values);
    void set_color(Color color) { color_ = color; }
    void set_marker(bool on) { marker_ = on; }
    void set_zero_baseline(bool on) { zero_baseline_ = on; }

    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }

    // 返回实际写入画布的格数（画布为空或数据不足 2 点时返回 0）。
    [[nodiscard]] TERMCHART_API std::size_t draw(canvas::Canvas& canvas) const;

private:
    std::vector<double> values_;
    Color color_ = Color::ansi16(6);
    bool marker_ = true;
    bool zero_baseline_ = false;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_SPARKLINE_HPP
