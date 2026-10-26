// bar_chart.hpp —— 笛卡尔柱状图（单系列，垂直柱）
//
// 与 LineChart 同构：网格 → 轴 → 从基线起按值填充竖条（█）→ 图例 → 标题。
// 分组/堆叠在多系列场景才需要，作为后续扩展（见 ROADMAP）。
#ifndef TERMCHART_CHART_CARTESIAN_BAR_CHART_HPP
#define TERMCHART_CHART_CARTESIAN_BAR_CHART_HPP

#include <optional>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/scale/auto_domain.hpp"

namespace termchart::chart {

class BarChart final : public Chart {
public:
    // 柱用单系列即可；values 为每个类别的值，categories 为对应名称。
    void set_values(std::vector<double> values, std::vector<std::string> categories,
                    Color color = default_color(DefaultColor::Accent), std::string name = "series");
    void set_show_legend(bool on) { show_legend_ = on; }

    // 显式 Y 域（权威值，优先于数据域）。**柱的基线是 0**：域里不含 0 时柱会
    // 从轴底开始长，视觉上把"截断轴"这个谎固化进图里，所以调用方必须自己把 0
    // 并进域内（autoviz 的稳健域就这么做）。域外的值由比例尺钳到轴边缘。
    void set_y_domain(double lo, double hi) { y_domain_ = scale::Domain{lo, hi}; }
    [[nodiscard]] bool has_y_domain() const noexcept { return y_domain_.has_value(); }

    void draw(DrawContext ctx) override;

    // 真实 Scene 通路：柱是**实心矩形**（FillOp + RectOp），终端侧由 render_scene
    // 光栅成半块实心格，导出侧就是 <rect fill>——不是逐格 █ 的转录。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    // 整组柱子是组 0：柱子从基线长高交给 `apply_animation` 按组进度做。
    void animation_groups(std::vector<GroupProgress>& out) const override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> values_;
    std::vector<std::string> categories_;
    Color color_ = default_color(DefaultColor::Accent);
    std::string name_ = "series";
    bool show_legend_ = true;
    std::optional<scale::Domain> y_domain_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_BAR_CHART_HPP
