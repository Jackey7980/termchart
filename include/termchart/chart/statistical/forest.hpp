// forest.hpp —— 森林图（各研究点估计 + 置信区间横线）
//
// 字符化策略：每行一条研究——CI 用 '─' 横线、点估计落 '●'、合计行落 '◆'；
// x = 0 处画竖直 '·' 参考线（无效应基线）。统计口径：CI 直接按给定
// [lower, upper] 画，不做再计算；x 域自动并入 0，保证参考线在场。
#ifndef TERMCHART_CHART_STATISTICAL_FOREST_HPP
#define TERMCHART_CHART_STATISTICAL_FOREST_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

struct ForestStudy {
    std::string label;
    double estimate = 0.0;
    double lower = 0.0;  // CI 下界
    double upper = 0.0;  // CI 上界
};

struct ForestOptions {
    Color color = Color::ansi16(4);
    bool show_zero_line = true;
};

class ForestChart final : public Chart {
public:
    TERMCHART_API void set_studies(std::vector<ForestStudy> studies);
    // 可选合计行（常为菱形），画在最后；不设则不画。
    void set_total(ForestStudy total) { total_ = std::move(total); has_total_ = true; }

    void set_options(ForestOptions options) { options_ = options; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<ForestStudy> studies_;
    ForestStudy total_{};
    bool has_total_ = false;
    ForestOptions options_{};
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_STATISTICAL_FOREST_HPP
