// hit_region.hpp —— 通用命中区域（chart → interaction 的唯一契约）
//
// 交互层要能判断"点到了哪个图元"，但不能认识任何具体图表类型。解决办法：
// chart 侧登记若干 HitRegion（形状 + 几何 + **不透明 payload id**），交互层只
// 按形状做几何判定并回报 id。于是 interaction 不需要 switch 图表名——这正是
// ROADMAP 约束"hit_test 只 switch HitRegion::Shape，禁图表名"的落点。
#ifndef TERMCHART_CHART_HIT_REGION_HPP
#define TERMCHART_CHART_HIT_REGION_HPP

#include <cstdint>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

enum class HitShape {
    Rect,
    Circle,
    Polygon,
};

class HitRegion {
public:
    HitRegion() = default;

    static HitRegion rect(Rect bounds, std::uint32_t payload = 0);
    static HitRegion circle(Point center, double radius, std::uint32_t payload = 0);
    static HitRegion polygon(std::vector<Point> points, std::uint32_t payload = 0);

    [[nodiscard]] HitShape shape() const noexcept { return shape_; }
    // 不透明标识：interaction 只负责原样回报，不解释含义。
    [[nodiscard]] std::uint32_t payload() const noexcept { return payload_; }
    [[nodiscard]] const Rect& bounds() const noexcept { return bounds_; }

    [[nodiscard]] TERMCHART_API bool contains(double x, double y) const noexcept;

private:
    HitShape shape_ = HitShape::Rect;
    Rect bounds_{};
    double radius_ = 0.0;
    std::vector<Point> points_;
    std::uint32_t payload_ = 0;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_HIT_REGION_HPP
