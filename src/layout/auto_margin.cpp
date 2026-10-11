// auto_margin.cpp —— 自动留白实现
#include "termchart/layout/auto_margin.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/util/wcwidth.hpp"

namespace termchart::layout {
namespace {

double ceil_cells(double value) noexcept {
    return std::ceil(value);  // 字符网格上没有半格
}

double widest(const std::vector<std::string>& labels) noexcept {
    double best = 0.0;
    for (const std::string& label : labels) {
        best = std::max(best, static_cast<double>(util::display_width(label)));
    }
    return best;
}

}  // namespace

Margin compute_margins(const MarginInputs& inputs) noexcept {
    const double padding = inputs.padding > 0.0 ? inputs.padding : 0.0;

    const double left =
        padding + static_cast<double>(inputs.y_tick_label_width) +
        (inputs.y_tick_label_width > 0 ? static_cast<double>(inputs.y_tick_label_gap) : 0.0);

    const double bottom =
        padding + static_cast<double>(inputs.x_tick_label_height) +
        (inputs.x_tick_label_height > 0 ? static_cast<double>(inputs.x_tick_label_gap) : 0.0) +
        static_cast<double>(inputs.axis_label_rows);

    const double top = padding + static_cast<double>(inputs.title_lines);

    // 右侧两件事取大者：最后一个 x 刻度标签的一半（它是居中占位的），
    // 以及图例块（含与绘图区的空隙）。
    const double half_last_x_label =
        inputs.x_tick_label_width > 0
            ? std::ceil(static_cast<double>(inputs.x_tick_label_width) / 2.0)
            : 0.0;
    const double legend =
        inputs.legend_width > 0
            ? static_cast<double>(inputs.legend_width) +
                  static_cast<double>(inputs.legend_gap)
            : 0.0;
    const double right = padding + std::max(half_last_x_label, legend);

    Margin margin;
    margin.left = ceil_cells(left);
    margin.right = ceil_cells(right);
    margin.top = ceil_cells(top);
    margin.bottom = ceil_cells(bottom);
    return margin;
}

Margin compute_margins_for(const std::vector<std::string>& y_tick_labels,
                           const std::vector<std::string>& x_tick_labels,
                           const MarginInputs& base) {
    MarginInputs inputs = base;
    inputs.y_tick_label_width = static_cast<std::uint32_t>(
        std::max(0.0, widest(y_tick_labels)));
    inputs.x_tick_label_width =
        static_cast<std::uint32_t>(std::max(0.0, widest(x_tick_labels)));
    // x 刻度标签只要有内容，就至少占一行（调用方若显式给了行数则以它为准）。
    if (!x_tick_labels.empty() && inputs.x_tick_label_height == 0) {
        inputs.x_tick_label_height = 1;
    }
    return compute_margins(inputs);
}

}  // namespace termchart::layout
