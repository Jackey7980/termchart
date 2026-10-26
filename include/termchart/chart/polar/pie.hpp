// pie.hpp —— 饼图
//
// 字符化策略：圆盘 = 绘图区内切椭圆的**格粒度解析填充**——逐格算归一化半径
// （≤ 1 才落笔），与 graph/shape 的 Ellipse 同一哲学：曲线在格粒度下必然有台阶，
// 不做假精度。扇区不依赖颜色区分（单色终端仍可读）：每个扇区从阶梯表取**专属
// 填充字符**（█▓▒░■●▲◆○ 循环），扇区边界即字符边界。角度从 start_angle（度，
// 数学方向，90 = 正上）起按 clockwise 方向扫过，格的归属角 = 起始方向起扫过的角。
#ifndef TERMCHART_CHART_POLAR_PIE_HPP
#define TERMCHART_CHART_POLAR_PIE_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"

namespace termchart::chart {

struct PieOptions {
    double start_angle = 90.0;  // 起始方向（度，数学方向：0=东，90=北）
    bool clockwise = true;      // 扫角方向
    bool show_legend = true;
};

class PieChart final : public Chart {
public:
    // values 为各类别的量（非有限/非正值跳过），labels 与之一一对应（可缺省）。
    void set_values(std::vector<double> values, std::vector<std::string> labels = {});
    void set_options(const PieOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    std::vector<double> values_;
    std::vector<std::string> labels_;
    PieOptions options_{};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_POLAR_PIE_HPP
