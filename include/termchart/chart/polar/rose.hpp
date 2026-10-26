// rose.hpp —— 南丁格尔玫瑰（等角扇区，半径编码数值）
//
// 字符化策略：与饼图同款的格粒度解析填充，差别在半径——各扇区等角（2π/n），
// 扇区 i 的落笔判据是 rnorm ≤ value_i / vmax（半径 ∝ 数值，面积夸大是本图种
// 的表达意图）。扇区用专属字符阶梯（█▓▒░■●▲◆○ 循环）+ 调色板色区分；
// DrawContext::draw_grid 关掉时连外圈参考环也不画。
#ifndef TERMCHART_CHART_POLAR_ROSE_HPP
#define TERMCHART_CHART_POLAR_ROSE_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"

namespace termchart::chart {

struct RoseOptions {
    double start_angle = 90.0;  // 起始方向（度，数学方向：0=东，90=北）
    bool clockwise = true;      // 扫角方向
    bool show_legend = true;
};

class RoseChart final : public Chart {
public:
    void set_values(std::vector<double> values, std::vector<std::string> labels = {});
    void set_options(const RoseOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    std::vector<double> values_;
    std::vector<std::string> labels_;
    RoseOptions options_{};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_POLAR_ROSE_HPP
