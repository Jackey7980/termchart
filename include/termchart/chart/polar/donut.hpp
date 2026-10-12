// donut.hpp —— 环形图（带中心孔的饼图）
//
// 字符化策略：在饼图的格粒度解析填充上加一道**环带判定**——归一化半径
// rnorm ≤ 1（外缘）且 rnorm ≥ hole_ratio（内孔）的格才落笔；内孔刻意留空，
// 中心读数与背景因此可见。扇区仍用专属字符阶梯区分（█▓▒░■●▲◆○ 循环），
// 角度约定与 pie 一致（start_angle 度、clockwise 扫角）。
#ifndef TERMCHART_CHART_POLAR_DONUT_HPP
#define TERMCHART_CHART_POLAR_DONUT_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"

namespace termchart::chart {

struct DonutOptions {
    double start_angle = 90.0;   // 起始方向（度，数学方向：0=东，90=北）
    bool clockwise = true;       // 扫角方向
    double hole_ratio = 0.45;    // 内孔半径占比（归一化半径），钳到 [0, 0.9)
    bool show_legend = true;
};

class DonutChart final : public Chart {
public:
    void set_values(std::vector<double> values, std::vector<std::string> labels = {});
    void set_options(const DonutOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    std::vector<double> values_;
    std::vector<std::string> labels_;
    DonutOptions options_{};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_POLAR_DONUT_HPP
