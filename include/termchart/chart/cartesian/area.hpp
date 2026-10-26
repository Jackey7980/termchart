// area.hpp —— 笛卡尔面积图
//
// 与折线图同构：网格 → 坐标轴+刻度 → 折线轮廓 + 向 baseline 的逐列填充 →
// 图例 → 标题。填充按"列"做（每列求出曲线高度，填 [曲线, 基线) 之间的格），
// 与格粒度画布天然对齐，也直接复用 canvas 的既有原语，不自写扫描线。
//
// 单系列：面积图的堆叠/百分比/流图变体是后续扩展（见 ROADMAP），这里只做
// 最基础的"折线到基线"一档。
#ifndef TERMCHART_CHART_CARTESIAN_AREA_HPP
#define TERMCHART_CHART_CARTESIAN_AREA_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/scale/auto_domain.hpp"

namespace termchart::chart {

class AreaChart final : public Chart {
public:
    TERMCHART_API void set_values(std::vector<Point> points,
                                  Color color = default_color(DefaultColor::Accent),
                                  std::string name = "series");

    // 填充基线（数据坐标）。默认 0：最常见的"从零起"面积图。改它会同时改
    // 定义域（基线被纳入 y 域）与填充落点。
    void set_baseline(double baseline) { baseline_ = baseline; }
    // 填充字符。默认低调的 '·'：不与折线/标记抢视觉，也便于测试单独计数。
    void set_fill_char(char32_t cp) { fill_char_ = cp; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 真实 Scene 输出（参考 LineChart/BarChart）：轴/网格/图例/标题走 TextOp，
    // 折线走 MoveTo/LineTo + StrokeOp（矢量），逐列填充走 RectOp + FillOp。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    // 暴露最后使用的绘图区，供测试断言几何。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Point> points_;
    Color color_ = default_color(DefaultColor::Accent);
    std::string name_ = "series";
    double baseline_ = 0.0;
    char32_t fill_char_ = U'\u00B7';
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_AREA_HPP
