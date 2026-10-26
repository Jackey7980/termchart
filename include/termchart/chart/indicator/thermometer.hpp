// thermometer.hpp —— 温度计
//
// 竖管 + 底部球泡 + 右侧刻度。液柱高度按 (value - vmin) / (vmax - vmin)
// 线性映射，液色随填充比例分档（青→绿→黄→红）。值收敛到域内，非有限
// 值只画空管不画液柱，不崩溃。
#ifndef TERMCHART_CHART_INDICATOR_THERMOMETER_HPP
#define TERMCHART_CHART_INDICATOR_THERMOMETER_HPP

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct ThermometerOptions {
    double vmin = 0.0;
    double vmax = 100.0;
    int ticks = 5;    // 刻度数，收敛到 [0, 10]
};

class Thermometer final : public Chart {
public:
    void set_value(double value) { value_ = value; }
    void set_options(const ThermometerOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

    // 真实 Scene 输出：管/球泡/液柱/刻度均以 TextOp 落笔，整体包在组 0。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    double value_ = 0.0;
    ThermometerOptions options_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_INDICATOR_THERMOMETER_HPP
