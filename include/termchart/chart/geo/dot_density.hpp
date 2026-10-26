// dot_density.hpp —— 点密度（示意地图级别）
//
// **精度立场**：点只落在调用方给定的区域格集合内，不做真实投影、不做
// 随机抖动（确定性优先），**不做假精度**。每 unit 单位数值画一个点
// （'•'），一格至多一个点；点数超出格数时截断到格数。
#ifndef TERMCHART_CHART_GEO_DOT_DENSITY_HPP
#define TERMCHART_CHART_GEO_DOT_DENSITY_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct DotDensityRegion {
    std::string name;
    std::vector<Point> cells;   // 字符格坐标（画布格）
    double value = 0.0;
    Color color = default_color(DefaultColor::Accent);
};

struct DotDensityOptions {
    double unit = 1.0;   // 每多少数值画一个点，收敛到 > 0
};

class DotDensity final : public Chart {
public:
    void add_region(DotDensityRegion region) { regions_.push_back(std::move(region)); }
    void set_options(const DotDensityOptions& options) { options_ = options; }

    [[nodiscard]] std::size_t region_count() const noexcept { return regions_.size(); }

    void draw(DrawContext ctx) override;
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    std::vector<DotDensityRegion> regions_;
    DotDensityOptions options_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_GEO_DOT_DENSITY_HPP
