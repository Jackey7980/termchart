// ecdf.hpp —— 经验分布函数（ECDF）
//
// 字符化策略：升序样本逐级"跳台阶"——水平段用 '─'、垂直抬升用 '│'、每个
// 样本在 (x_i, i/n) 处落 '●'；统计口径：F̂(x) = #{x_i ≤ x}/n，n 为有限样本
// 数，重复值合并为一次完整抬升。y 域固定 [0, 1]。
#ifndef TERMCHART_CHART_STATISTICAL_ECDF_HPP
#define TERMCHART_CHART_STATISTICAL_ECDF_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

struct EcdfOptions {
    bool show_risers = true;   // 台阶间的垂直抬升线
    bool show_markers = true;  // 每个样本处的 '●'
};

class EcdfChart final : public Chart {
public:
    TERMCHART_API void set_values(std::vector<double> values,
                                  Color color = default_color(DefaultColor::Neutral),
                                  std::string name = "ECDF");

    void set_options(EcdfOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }
    // 升序有限样本；无有效样本时为空。
    [[nodiscard]] const std::vector<double>& sorted_values() const noexcept { return sorted_; }

private:
    std::vector<double> values_;
    std::vector<double> sorted_;
    Color color_ = default_color(DefaultColor::Neutral);
    std::string name_ = "ECDF";
    EcdfOptions options_{};
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_STATISTICAL_ECDF_HPP
