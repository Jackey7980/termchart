// heatmap.hpp —— 笛卡尔热力图（二维点集 → 网格 → 计数 → 连续色表着色）
//
// 与"矩阵热力图"不同：输入是**散点坐标**而非规整矩阵，因此要先落到一个网格
// （由数据范围与单元边长决定），统计每格点数，再把计数归一化后用连续色表上色。
// 密度同时由"字符"与"颜色"两条通道编码——单一字符表达不了连续量，单一颜色在
// 低对比终端上也可能不可读；两者叠加才既可见又可比。
//
// 网格**先算后画**：先把所有点落格得到计数矩阵（索引一律 clamp，绝不出现负索引
// 折回），再把非空格画进画布、并对每个格矩形与内框求交做边界裁剪。
#ifndef TERMCHART_CHART_CARTESIAN_HEATMAP_HPP
#define TERMCHART_CHART_CARTESIAN_HEATMAP_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/core/point.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

class HeatmapChart final : public Chart {
public:
    // points 为数据坐标下的散点；kind 指定连续色表；name 为图例名。
    TERMCHART_API void set_values(std::vector<Point> points,
                                  color::ColormapKind kind = color::ColormapKind::Viridis,
                                  std::string name = "heatmap");

    // 网格单元在**数据坐标**下的边长（x/y 同值）。传 <=0 表示自动（按点数选）。
    void set_cell_size(double size) { cell_size_ = size; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 暴露最后使用的绘图区，供测试断言几何（含定义域端点有限性）。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Point> points_;
    color::ColormapKind kind_ = color::ColormapKind::Viridis;
    std::string name_ = "heatmap";
    double cell_size_ = 0.0;  // <=0：自动
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_HEATMAP_HPP
