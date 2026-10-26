// volume.hpp —— 成交量图（基线起柱，方向色）
//
// 字符化策略：每根柱占一个槽位、从量基线起填 █，高度 ∝ 成交量；
// 方向表（涨/跌）存在时分别用 up/down 色，缺省用中性灰。
#ifndef TERMCHART_CHART_FINANCIAL_VOLUME_HPP
#define TERMCHART_CHART_FINANCIAL_VOLUME_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

struct VolumeOptions {
    Color up = default_color(DefaultColor::Accent);    // 价涨柱
    Color down = default_color(DefaultColor::Warn);  // 价跌柱
    Color neutral = default_color(DefaultColor::Muted);
};

class VolumeChart final : public Chart {
public:
    // dirs 可空或与 values 等长：>0 涨、<0 跌、0/缺省中性。
    void set_values(std::vector<double> values, std::vector<int> dirs = {});

    void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    VolumeOptions options;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> values_;
    std::vector<int> dirs_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_FINANCIAL_VOLUME_HPP
