// bubble.hpp —— 气泡图：散点 + 第三维（半径）
//
// 终端字符化策略：终端格没有"面积"可缩放，第三维用**点阵符号的大小阶梯**
// 表达——r 归一化到 [rmin, rmax] 后取 · → • → ● 三档，越大越显眼；
// 全体同值时取中档（此时第三维不携带信息）。
#ifndef TERMCHART_CHART_CARTESIAN_BUBBLE_HPP
#define TERMCHART_CHART_CARTESIAN_BUBBLE_HPP

#include <string>
#include <utility>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

class BubbleChart final : public Chart {
public:
    struct Bubble {
        Point p{};
        double r = 0.0;  // 第三维：非负半径语义，负值/非有限按 0 处理
    };
    struct Options {
        Color color = Color::ansi16(4);
        std::string name = "bubble";
        bool show_legend = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<Bubble> bubbles) { bubbles_ = std::move(bubbles); }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 暴露最后使用的绘图区，供测试断言几何。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<Bubble> bubbles_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_BUBBLE_HPP
