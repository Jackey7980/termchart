// radar.hpp —— 雷达图（多轴蛛网 + 数据多边形）
//
// 字符化策略：蛛网的射线与环全部走 DDA 线段离散（canvas::line_points，与
// graph/shape 六边形同法）——**环是缩小的多边形**（连接各轴在 1/3、2/3、1
// 半径处的点），不是圆；数据多边形用 '•' 描边、'●' 标顶点，蛛网用 '·'，
// 三种字符互不相同，单色终端也能分层读。轴标签画在外顶点外推 1.15 半径处
// （越界由 canvas 裁剪）。DrawContext::draw_grid 关掉时不画蛛网。
#ifndef TERMCHART_CHART_POLAR_RADAR_HPP
#define TERMCHART_CHART_POLAR_RADAR_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"

namespace termchart::chart {

struct RadarOptions {
    double start_angle = 90.0;  // 第一根轴的方向（度，90 = 正上）
    bool clockwise = true;      // 轴的排布方向
    bool show_legend = true;
};

class RadarChart final : public Chart {
public:
    // 每根轴一个值（非有限值按 0 处理）与一个可选标签；少于 3 根轴不成多边形。
    void set_values(std::vector<double> values, std::vector<std::string> labels = {});
    void set_series_name(std::string name) { name_ = std::move(name); }
    void set_color(Color color) { color_ = color; }
    void set_options(const RadarOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    std::vector<double> values_;
    std::vector<std::string> labels_;
    std::string name_ = "series";
    Color color_ = default_color(DefaultColor::Accent);
    RadarOptions options_{};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_POLAR_RADAR_HPP
