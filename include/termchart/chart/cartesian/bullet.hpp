// bullet.hpp —— 子弹图：每行一条水平刻度条，定性背景带 + 实心度量条 + 目标竖线。
//
// 终端字符化策略：背景带用 ░▒▓ 三档明暗区分定性区间，度量值用 █ 实心条，目标用 │ 竖线，
// 全部信息靠字符明暗与位置表达，不依赖颜色。
#ifndef TERMCHART_CHART_CARTESIAN_BULLET_HPP
#define TERMCHART_CHART_CARTESIAN_BULLET_HPP

#include <array>
#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

class BulletChart final : public Chart {
public:
    struct Row {
        std::string label;
        double value = 0.0;                  // 度量值（实心条）
        double target = 0.0;                 // 目标值（竖线）
        std::array<double, 3> bands{};       // 三段定性背景带的递增右端点
    };

    struct Options {
        char32_t target_char = U'\u2502';    // │
        Color bar_color = default_color(DefaultColor::Neutral);
        bool show_legend = false;
    };

    TERMCHART_API void set_rows(std::vector<Row> rows);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Row> rows_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_BULLET_HPP
