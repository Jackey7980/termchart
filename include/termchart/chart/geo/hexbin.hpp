// hexbin.hpp —— 六边形分箱（示意地图级别）
//
// **精度立场**：偏移网格近似六边形铺排（奇数行右移半径一半），不是几何
// 精确的正六边形，也不做真实投影，**不做假精度**。输入点 (x, y) 落进最近
// 的六边形桶，桶内计数再按 bins 档分级着色（计数越多越深）。
#ifndef TERMCHART_CHART_GEO_HEXBIN_HPP
#define TERMCHART_CHART_GEO_HEXBIN_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct HexbinOptions {
    double radius = 2.0;   // 桶半径（格），收敛到 >= 1
    int bins = 5;          // 计数分级数，收敛到 [1, 8]
};

class Hexbin final : public Chart {
public:
    // 非有限坐标的点在 draw 时被跳过，不崩溃。
    void add_point(double x, double y) { points_.push_back(Point{x, y}); }
    void set_options(const HexbinOptions& options) { options_ = options; }

    [[nodiscard]] std::size_t point_count() const noexcept { return points_.size(); }

    void draw(DrawContext ctx) override;

private:
    std::vector<Point> points_;
    HexbinOptions options_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_GEO_HEXBIN_HPP
