// step.hpp —— 笛卡尔阶梯图
//
// 与折线图同构，唯一区别是相邻点之间不直连，而走"横—竖—横"台阶：数据在采样
// 区间内保持某一档（前一档/后一档/中点换档），而不是线性过渡。适合表达离散
// 状态、累计量台阶、保持型信号等。三档语义见下面的 Where。
#ifndef TERMCHART_CHART_CARTESIAN_STEP_HPP
#define TERMCHART_CHART_CARTESIAN_STEP_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/scale/auto_domain.hpp"

namespace termchart::chart {

class StepChart final : public Chart {
public:
    // 台阶落在哪一侧：
    //   Post —— 先水平后垂直：采样区间保持"前一点"的值（最常用的阶梯，默认）；
    //   Pre  —— 先垂直后水平：采样区间保持"后一点"的值；
    //   Mid  —— 在两点中点处换档，两侧各留半段台阶。
    enum class Where { Pre, Post, Mid };

    TERMCHART_API void set_values(std::vector<Point> points,
                                  Color color = Color::ansi16(2),
                                  std::string name = "series");
    void set_where(Where where) { where_ = where; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 暴露最后使用的绘图区，供测试断言几何。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Point> points_;
    Color color_ = Color::ansi16(2);
    std::string name_ = "series";
    Where where_ = Where::Post;
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_STEP_HPP
