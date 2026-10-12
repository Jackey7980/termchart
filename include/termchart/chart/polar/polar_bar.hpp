// polar_bar.hpp —— 极坐标柱状图（圆形排布的径向柱）
//
// 字符化策略：柱 = 角域 × 半径域的格粒度解析填充——格落在第 i 个槽位
// （等角 2π/n 的中央 bar_width 比例）且 rnorm ≤ value_i/vmax 才写 '█'，
// 与 graph/shape 的椭圆同一哲学：格粒度下必有台阶，不做假精度。参考环
// （0.25/0.5/0.75/1.0 半径）与槽位射线用 '·' 沿角度采样画（DrawContext::
// draw_grid 开关控制）；没有直角坐标轴，DrawContext::draw_axes 被忽略。
#ifndef TERMCHART_CHART_POLAR_POLAR_BAR_HPP
#define TERMCHART_CHART_POLAR_POLAR_BAR_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"

namespace termchart::chart {

struct PolarBarOptions {
    double start_angle = 0.0;   // 第一个槽位的起始方向（度，0 = 东）
    bool clockwise = true;      // 槽位排布方向
    double bar_width = 0.8;     // 柱占槽位的角宽比例，钳到 (0, 1]
    bool show_legend = true;
};

class PolarBarChart final : public Chart {
public:
    void set_values(std::vector<double> values, std::vector<std::string> categories = {});
    void set_options(const PolarBarOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    std::vector<double> values_;
    std::vector<std::string> categories_;
    PolarBarOptions options_{};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_POLAR_POLAR_BAR_HPP
