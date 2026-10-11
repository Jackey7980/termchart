// clip.hpp —— 裁剪区域（矩形与凸多边形）
//
// 两种区域：
//   * 矩形：最常用。线段裁剪走精确的 Liang–Barsky；多边形裁剪走 4 个半平面。
//   * **凸**多边形：逐点判定 + Sutherland–Hodgman 逐边裁剪。
//
// 非凸多边形**只支持逐点判定**：多边形裁剪会原样返回，且 supports_polygon_clip()
// 报 false。这是刻意的——Sutherland–Hodgman 对非凸裁剪区会给出看似合理实则错误
// 的结果，宁可在接口上明说，也不让调用方拿到错的形状。
#ifndef TERMCHART_CANVAS_CLIP_HPP
#define TERMCHART_CANVAS_CLIP_HPP

#include <cstddef>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {

class ClipRegion {
public:
    ClipRegion() = default;  // 无约束：全部放行

    [[nodiscard]] static ClipRegion rect(const Rect& bounds);
    // 传入的点列会被规范化为逆时针（闭合与否都接受，内部按闭合处理）。
    [[nodiscard]] static ClipRegion polygon(std::vector<Point> points);

    // 没有任何约束（contains 恒真、裁剪恒返回原值）。
    [[nodiscard]] bool unconstrained() const noexcept { return !has_rect_ && !has_polygon_; }
    [[nodiscard]] bool is_rect() const noexcept { return has_rect_; }

    [[nodiscard]] TERMCHART_API bool contains(Point p) const noexcept;

    // 仅当 is_rect() 时有意义；否则返回零矩形。
    [[nodiscard]] Rect rect_bounds() const noexcept { return rect_; }

    // 是否支持多边形裁剪（矩形恒支持；多边形区域要求凸）。
    [[nodiscard]] bool supports_polygon_clip() const noexcept;

    // 裁剪多边形：矩形与凸多边形走 Sutherland–Hodgman；不支持时原样返回。
    [[nodiscard]] TERMCHART_API std::vector<Point> clip_polygon(
        std::vector<Point> polygon) const;

    // 裁剪线段：完全落在区域外返回 false，否则 out_a/out_b 为裁剪后的端点。
    [[nodiscard]] TERMCHART_API bool clip_segment(Point a, Point b, Point& out_a,
                                                  Point& out_b) const noexcept;

private:
    // 半平面裁剪的单步：把 input 裁到"边 (edge_a → edge_b) 的左侧"。
    void clip_half_plane(std::vector<Point>& poly, Point edge_a, Point edge_b,
                         bool keep_left) const;

    bool has_rect_ = false;
    Rect rect_{};
    bool has_polygon_ = false;
    std::vector<Point> polygon_;  // 逆时针
    bool polygon_convex_ = false;
};

}  // namespace termchart::canvas

#endif  // TERMCHART_CANVAS_CLIP_HPP
