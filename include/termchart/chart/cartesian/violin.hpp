// violin.hpp —— 小提琴图：核密度轮廓
//
// 终端字符化策略：每组样本先做高斯核密度估计（Silverman 带宽），再沿 y 方向
// 切成若干水平带，每带的密度映射为**以组中线为轴的对称 █ 条带宽度**——
// 密度越高条带越宽，轮廓自然呈小提琴形；中位数画 ─。
#ifndef TERMCHART_CHART_CARTESIAN_VIOLIN_HPP
#define TERMCHART_CHART_CARTESIAN_VIOLIN_HPP

#include <string>
#include <utility>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

class ViolinChart final : public Chart {
public:
    struct Options {
        Color color = Color::ansi16(4);
        int bands = 0;  // 密度带数；0（或负）= 自动（随绘图区高度，clamp 到 [4, 64]）
        bool show_median = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    // groups[g] 为第 g 组的原始样本；空组/全非有限组在 draw 时整体跳过。
    void set_data(std::vector<std::vector<double>> groups) {
        groups_ = std::move(groups);
    }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<std::vector<double>> groups_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_VIOLIN_HPP
