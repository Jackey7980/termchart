// autocorr.hpp —— 自相关函数（ACF）
//
// 字符化策略：每个滞后期一根棒棒糖——零线到 r_k 的竖线 '│'、顶端 '●'；
// ±1.96/√n 置信带画水平 '·' 参考线。统计口径：r_k = Σ(x_i-x̄)(x_{i-k}-x̄) /
// Σ(x_i-x̄)²，k = 1..max_lag（clamp 到 n-1），样本方差为 0（常数序列）或
// n < 2 时不绘制。
#ifndef TERMCHART_CHART_STATISTICAL_AUTOCORR_HPP
#define TERMCHART_CHART_STATISTICAL_AUTOCORR_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

struct AutocorrOptions {
    int max_lag = 16;       // 最大滞后期，内部 clamp 到 [1, n-1]
    bool show_band = true;  // ±1.96/√n 置信带
};

class AutocorrChart final : public Chart {
public:
    TERMCHART_API void set_values(std::vector<double> values,
                                  Color color = default_color(DefaultColor::Neutral));

    void set_options(AutocorrOptions options) { options_ = options; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    // r[k] 为滞后 k 的自相关（k = 0..实际最大滞后；r[0] = 1；未算成为空）。
    [[nodiscard]] const std::vector<double>& acf() const noexcept { return acf_; }
    // 置信带半宽 1.96/√n（未算成为 0）。
    [[nodiscard]] double conf_band() const noexcept { return conf_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> values_;
    Color color_ = default_color(DefaultColor::Neutral);
    AutocorrOptions options_{};
    std::vector<double> acf_;
    double conf_ = 0.0;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_STATISTICAL_AUTOCORR_HPP
