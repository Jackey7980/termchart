// polar_line.hpp —— 极坐标折线（角度 × 半径的数据序列连成线）
//
// 字符化策略：数据点 (角度°, 半径) 先映射到格坐标（角度从 start_angle 起沿
// clockwise 方向扫、半径除以 rmax 归一化成 rnorm），相邻点之间走 DDA 线段
// 离散（canvas::line_points）写 '•'，数据点本身压 '●'；外圈参考环用 '·'
// 沿角度采样（DrawContext::draw_grid 开关）。格粒度下线必有台阶，不做假精度。
#ifndef TERMCHART_CHART_POLAR_POLAR_LINE_HPP
#define TERMCHART_CHART_POLAR_POLAR_LINE_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"

namespace termchart::chart {

struct PolarLineOptions {
    double start_angle = 0.0;   // 角度 0 的方向（度，0 = 东）
    bool clockwise = true;      // 角度增长方向
    bool close = false;         // 末点与首点是否相连（成环）
    double rmax = 0.0;          // 半径满量程；≤ 0 时取数据最大值
    bool show_markers = true;   // 数据点是否压 '●'
};

class PolarLineChart final : public Chart {
public:
    // 每个点 p.x = 角度（度，图表角），p.y = 半径；非有限点跳过。
    void set_points(std::vector<Point> points) { points_ = std::move(points); }
    void set_color(Color color) { color_ = color; }
    void set_options(const PolarLineOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    std::vector<Point> points_;
    Color color_ = default_color(DefaultColor::Accent);
    PolarLineOptions options_{};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_POLAR_POLAR_LINE_HPP
