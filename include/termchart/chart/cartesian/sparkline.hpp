// sparkline.hpp —— 迷你图：单行趋势线，逐列取下八分块 ▁▂▃…█。
//
// 终端字符化策略：min-max 归一化到 0..8 后逐列取下八分块字符，一行即成图，
// 可直接嵌进状态栏或表格行；末列可选 ● 标出最新值。
#ifndef TERMCHART_CHART_CARTESIAN_SPARKLINE_HPP
#define TERMCHART_CHART_CARTESIAN_SPARKLINE_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

class SparklineChart final : public Chart {
public:
    struct Options {
        Color color = default_color(DefaultColor::Accent);
        bool show_last_marker = true;
    };

    TERMCHART_API void set_values(std::vector<double> values);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    std::vector<double> values_;
    Options options_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_SPARKLINE_HPP
