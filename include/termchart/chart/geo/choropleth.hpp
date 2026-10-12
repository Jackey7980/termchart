// choropleth.hpp —— 分级统计图（示意地图级别）
//
// **精度立场**：区域由调用方给定的**字符格集合**表达，不做真实地理投影、
// 不读取边界数据。格粒度轮廓有台阶是终端字符网格的固有限制，**不做假
// 精度**；本图只负责"值 → 分级色"这一步。分级：把区域值域均分成 bins
// 档，值越高灰阶越深（256 色灰阶段 232..255）。
#ifndef TERMCHART_CHART_GEO_CHOROPLETH_HPP
#define TERMCHART_CHART_GEO_CHOROPLETH_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct GeoRegion {
    std::string name;
    std::vector<Point> cells;   // 字符格坐标（画布格）
    double value = 0.0;
};

struct ChoroplethOptions {
    int bins = 5;   // 分级数，收敛到 [1, 8]
};

class Choropleth final : public Chart {
public:
    void add_region(GeoRegion region) { regions_.push_back(std::move(region)); }
    void set_options(const ChoroplethOptions& options) { options_ = options; }

    [[nodiscard]] std::size_t region_count() const noexcept { return regions_.size(); }

    void draw(DrawContext ctx) override;

private:
    std::vector<GeoRegion> regions_;
    ChoroplethOptions options_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_GEO_CHOROPLETH_HPP
