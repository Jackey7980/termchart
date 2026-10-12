// dumbbell.hpp —— 哑铃图：每行一个类别，同一行内两个数值之间用"线段 + 两端实心点"呈现差异。
//
// 终端字符化策略：行内差异用 • 连线、● 标两端值，一行一类别，不依赖颜色也能读出差距。
#ifndef TERMCHART_CHART_CARTESIAN_DUMBBELL_HPP
#define TERMCHART_CHART_CARTESIAN_DUMBBELL_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

class DumbbellChart final : public Chart {
public:
    struct Row {
        std::string label;  // 类别名（画在左侧留白，超宽被画布裁剪）
        double a = 0.0;     // 端点一
        double b = 0.0;     // 端点二
    };

    struct Options {
        Color line_color = Color::ansi16(8);  // 连线颜色（暗灰，突出端点）
        Color dot_color = Color::ansi16(4);
        bool show_legend = false;  // 单系列图例意义不大，默认关
    };

    TERMCHART_API void set_rows(std::vector<Row> rows);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Row> rows_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_DUMBBELL_HPP
