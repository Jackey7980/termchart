// gantt.hpp —— 甘特图：任务时间条 + 行标签
//
// 终端字符化策略：每个任务占一行，时间条是按 [start, end] 落在时间轴上的
// █ 条带（行内垂直居中）；行标签写在左侧留白里（按码点截断，不撕裂 UTF-8），
// 行间只画垂直时间网格线（横向网格线对任务行没有意义）。
#ifndef TERMCHART_CHART_CARTESIAN_GANTT_HPP
#define TERMCHART_CHART_CARTESIAN_GANTT_HPP

#include <string>
#include <utility>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

class GanttChart final : public Chart {
public:
    struct Task {
        std::string name;
        double start = 0.0;
        double end = 0.0;
    };
    struct Options {
        Color bar = default_color(DefaultColor::Neutral);
        bool show_grid = true;
        bool show_labels = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<Task> tasks) { tasks_ = std::move(tasks); }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 真实 Scene 输出：时间条用矢量 RectOp+FillOp 复刻，行标签走 TextOp；
    // 仅画时间网格 + x 轴（任务行是分类，无 y 轴），与 draw() 一致。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<Task> tasks_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_GANTT_HPP
