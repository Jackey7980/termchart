// progress_ring.hpp —— 进度环：单值环形
//
// **与 text/progress 的区分**（规格注明，勿混）：本图是**单值环形**读数
// （圆弧从顶部顺时针扫过 fraction 圈），text/progress 是横向条形组。
// 环在字符格粒度上用椭圆采样近似，格粒度有台阶，**不做假精度**。
#ifndef TERMCHART_CHART_INDICATOR_PROGRESS_RING_HPP
#define TERMCHART_CHART_INDICATOR_PROGRESS_RING_HPP

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct ProgressRingOptions {
    bool show_percent = true;
    Color track_color = default_color(DefaultColor::Muted);   // 未完成轨道
};

class ProgressRing final : public Chart {
public:
    // fraction 收敛到 [0, 1]，非有限按 0。
    void set_fraction(double fraction) { fraction_ = fraction; }
    void set_color(Color color) { color_ = color; }
    void set_options(const ProgressRingOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

    // 真实 Scene 输出：椭圆采样的环点（●/·）与中心百分比均以 TextOp 落笔。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    double fraction_ = 0.0;
    Color color_ = default_color(DefaultColor::Cyan);
    ProgressRingOptions options_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_INDICATOR_PROGRESS_RING_HPP
