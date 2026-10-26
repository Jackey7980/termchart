// survival.hpp —— 生存曲线（Kaplan-Meier 估计）
//
// 字符化策略：KM 阶梯自 1 递降——水平段 '─'、事件处垂直抬升（下降）'│'、
// 删失（censored）时刻落 '+' 刻度。统计口径：S(t) = ∏(1 - d_i/n_i)，d_i 为
// 事件数、n_i 为处于风险的个体数（time ≥ t_i），同刻事件合并一步；times 与
// events 按较短者取齐，非有限时间剔除。y 域固定 [0, 1]。
#ifndef TERMCHART_CHART_STATISTICAL_SURVIVAL_HPP
#define TERMCHART_CHART_STATISTICAL_SURVIVAL_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

struct SurvivalOptions {
    Color color = default_color(DefaultColor::Accent);
    bool show_censored = true;  // 删失 '+' 刻度
};

class SurvivalChart final : public Chart {
public:
    // times[i] 为观测时长，events[i] != 0 表示事件发生、0 表示删失。
    TERMCHART_API void set_data(std::vector<double> times, std::vector<int> events,
                                Color color = default_color(DefaultColor::Accent),
                                std::string name = "survival");

    void set_options(SurvivalOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    // KM 曲线（(时间, S) 数据坐标；无有效样本时为空）。
    [[nodiscard]] const std::vector<Point>& curve() const noexcept { return curve_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    SurvivalOptions options_{};
    bool show_legend_ = true;
    std::string name_ = "survival";
    std::vector<Point> curve_;
    std::vector<double> censored_;  // 删失时刻（升序）
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_STATISTICAL_SURVIVAL_HPP
