// errorbar.hpp —— 误差棒：中心点 + 不对称误差区间
//
// 终端字符化策略：数据点画 ●，y 误差区间是 │ 竖线 + ─ 端帽，x 误差区间是
// ─ 横线 + │ 端帽；误差上下界支持不对称（lo/hi 独立给定，lo>hi 自动交换）。
#ifndef TERMCHART_CHART_CARTESIAN_ERRORBAR_HPP
#define TERMCHART_CHART_CARTESIAN_ERRORBAR_HPP

#include <string>
#include <utility>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

class ErrorBarChart final : public Chart {
public:
    struct ErrorPoint {
        double x = 0.0;
        double y = 0.0;
        double y_lo = 0.0;  // y 误差下界；非有限表示该向无误差
        double y_hi = 0.0;
        double x_lo = 0.0;
        double x_hi = 0.0;
    };
    struct Options {
        Color color = default_color(DefaultColor::Neutral);
        bool show_caps = true;  // 误差端帽
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<ErrorPoint> points) { points_ = std::move(points); }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<ErrorPoint> points_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_ERRORBAR_HPP
