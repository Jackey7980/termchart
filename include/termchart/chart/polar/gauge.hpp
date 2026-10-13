// gauge.hpp —— 仪表盘（弧形刻度带 + 指针，含刻度）
//
// 字符化策略：弧 = 沿数学角**按角度采样**落格（与 canvas::arc_points 同法，
// 步长约每周 4·(rx+ry) 格保证连续；格粒度下必有台阶，不做假精度）。弧体用
// '●'，主刻度用 '+' 压在弧体与内侧（rnorm 0.88）两处，指针是从圆心到
// rnorm 0.72 的 DDA 线段（'*'），圆心一粒 '●' 作轴帽。数值域 [value_min,
// value_max] 线性映射到 [start_angle, end_angle] 的顺时针扫角（默认 225°→
// -45°，即经典 270° 开口朝下的表盘）。
#ifndef TERMCHART_CHART_POLAR_GAUGE_HPP
#define TERMCHART_CHART_POLAR_GAUGE_HPP

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"

namespace termchart::chart {

struct GaugeOptions {
    double value_min = 0.0;
    double value_max = 100.0;
    double start_angle = 225.0;  // 起始方向（度，数学方向：0=东，90=北）
    double end_angle = -45.0;    // 结束方向；clockwise 时扫角 = start - end
    bool clockwise = true;
    int tick_count = 9;          // 主刻度数（含两端）
    bool show_value = true;      // 在轴帽下方标注当前值
};

class GaugeChart final : public Chart {
public:
    void set_value(double value) { value_ = value; }
    void set_color(Color color) { color_ = color; }
    void set_options(const GaugeOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    double value_ = 0.0;
    Color color_ = default_color(DefaultColor::Accent);
    GaugeOptions options_{};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_POLAR_GAUGE_HPP
