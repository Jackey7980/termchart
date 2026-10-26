// ridgeline.hpp —— 山脊图：多组分布按行堆叠，密度轮廓用块条填充，行间部分重叠成"山脊"。
//
// 终端字符化策略：每组密度归一化后按列填充实心块（顶部分数行用下八分块），
// 后画的行覆盖先画的，天然形成前后遮挡的山脊观感。
#ifndef TERMCHART_CHART_CARTESIAN_RIDGELINE_HPP
#define TERMCHART_CHART_CARTESIAN_RIDGELINE_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

class RidgelineChart final : public Chart {
public:
    struct Group {
        std::string name;
        std::vector<double> values;
        Color color = default_color(DefaultColor::Series);
    };

    struct Options {
        int band_height = 4;   // 每组最大密度对应的行数（≥2）
        int bins = 24;         // 直方分箱数（clamp 到 [4,128]）
        bool show_legend = true;
    };

    TERMCHART_API void set_groups(std::vector<Group> groups);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 真实 Scene 输出：各组密度轮廓用矢量 RectOp+FillOp 复刻（分数行用薄矢量矩形
    // 代替 draw() 的八分块字符），组图例走 LegendEntry。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Group> groups_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_RIDGELINE_HPP
