// polar_heat.hpp —— 极坐标热力图（角向 × 径向分箱的密度上色）
//
// 字符化策略：把 (角度°, 半径) 点落入 angle_bins × radial_bins 的极坐标分箱，
// 填充仍是格粒度解析判定——格落在盘内（rnorm ≤ 1）即按其归属角/环查计数；
// 无数据的格留空（保留参考环），有数据的格用 4 级阴影字符（░▒▓█，计数越多
// 越实，最低档非空格以便与"无数据"区分）+ colormap 颜色。参考环/槽位射线
// 用 '·'（DrawContext::draw_grid 开关）；色标画在内框上方（沿用笛卡尔
// heatmap 的做法）。
#ifndef TERMCHART_CHART_POLAR_POLAR_HEAT_HPP
#define TERMCHART_CHART_POLAR_POLAR_HEAT_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"

namespace termchart::chart {

struct PolarHeatOptions {
    int angle_bins = 12;    // 角向分箱数，钳到 [1, 360]
    int radial_bins = 5;    // 径向分箱数，钳到 [1, 64]
    double rmax = 0.0;      // 半径满量程；≤ 0 时取数据最大值
    color::ColormapKind colormap = color::ColormapKind::Viridis;
    std::string name = "density";
    bool show_colorbar = true;
};

class PolarHeatChart final : public Chart {
public:
    // 每个点 p.x = 角度（度，图表角），p.y = 半径；非有限点跳过。
    void set_points(std::vector<Point> points) { points_ = std::move(points); }
    void set_options(const PolarHeatOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    std::vector<Point> points_;
    PolarHeatOptions options_{};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_POLAR_POLAR_HEAT_HPP
