// contour.hpp —— 等高线：标量场 → marching squares
//
// 终端字符化策略：对每条等值线跑 marching squares 提取折线段，段画成 · 连线
// （draw_line 自带裁剪与取整）。场按**行**给定且 row 0 = 最上 = ymax；等值线
// 级数缺省自动取值域内均分（不含端点），也可显式指定。
#ifndef TERMCHART_CHART_CARTESIAN_CONTOUR_HPP
#define TERMCHART_CHART_CARTESIAN_CONTOUR_HPP

#include <string>
#include <utility>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

class ContourChart final : public Chart {
public:
    struct Options {
        Color color = default_color(DefaultColor::Cyan);
        // 显式等值线级；非有限值剔除。为空时按 level_count 自动均分。
        std::vector<double> levels;
        int level_count = 4;  // 自动模式的级数，clamp 到 [1, 16]
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    // rows[j][i]：第 j 行第 i 列的场值，j=0 为最上（对应 ymax）。
    void set_field(std::vector<std::vector<double>> rows) { rows_ = std::move(rows); }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 真实 Scene 输出：marching squares 逐级提取的等值线段用矢量 StrokeOp 复刻，
    // 网格/轴/标题走 TextOp（与 AreaChart::to_scene 同构）。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<std::vector<double>> rows_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_CONTOUR_HPP
