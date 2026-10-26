// hist.hpp —— 笛卡尔直方图（原始样本 → 自动分箱 → 柱）
//
// 与柱状图同构，但输入是**未聚合的原始样本**：先自动分箱并统计每箱频数，再按箱
// 画柱。分箱规则内建而非由调用方给定，是因为"箱数该随样本量增长"本身就是直方图
// 的语义（样本越多、分辨率应越细）；调用方仍可用 set_bins 覆盖。分箱结果通过
// bin_edges()/bin_counts() 暴露，便于测试与上层复用（例如叠加密度曲线）。
#ifndef TERMCHART_CHART_CARTESIAN_HIST_HPP
#define TERMCHART_CHART_CARTESIAN_HIST_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

class HistChart final : public Chart {
public:
    // values 为原始样本（未分箱）。color/name 默认值沿用柱状图，便于并排绘制。
    TERMCHART_API void set_values(std::vector<double> values,
                                  Color color = default_color(DefaultColor::Accent),
                                  std::string name = "series");

    // 覆盖自动分箱的箱数；返回前会 clamp 到 [1, 64]。传 <=0 恢复自动（Sturges）。
    void set_bins(int bins) {
        bins_ = bins;
        rebin();
    }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 真实 Scene 输出：直方图柱走 RectOp + FillOp（矢量）。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    // 暴露最后使用的绘图区，供测试断言几何（含定义域端点有限性）。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

    // 最近一次输入对应的分箱结果（set_values/set_bins 后立即有效）。
    [[nodiscard]] std::size_t bin_count() const noexcept {
        return edges_.empty() ? 0 : edges_.size() - 1;
    }
    [[nodiscard]] const std::vector<double>& bin_edges() const noexcept { return edges_; }
    [[nodiscard]] const std::vector<std::size_t>& bin_counts() const noexcept {
        return counts_;
    }

private:
    // 由 set_values/set_bins 触发的确定性分箱（剔除非有限样本后重算）。
    void rebin();

    std::vector<double> values_;
    Color color_ = default_color(DefaultColor::Accent);
    std::string name_ = "series";
    int bins_ = 0;  // 0（或负）：自动（Sturges）
    bool show_legend_ = true;
    std::vector<double> edges_;          // bins+1 个边界（升序）
    std::vector<std::size_t> counts_;    // bins 个频数
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_HIST_HPP
