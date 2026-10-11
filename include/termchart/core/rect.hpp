// rect.hpp —— 浮点矩形（header-only）
//
// 约定：x/y 为左上角，宽高非负；所有关系谓词为闭-开区间（right/bottom 不含在内），
// 与 canvas 的半开像素约定一致。
#ifndef TERMCHART_CORE_RECT_HPP
#define TERMCHART_CORE_RECT_HPP

#include "termchart/core/point.hpp"

namespace termchart {

struct Rect {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;

    constexpr Rect() noexcept = default;
    constexpr Rect(double px, double py, double pw, double ph) noexcept
        : x(px), y(py), w(pw), h(ph) {}
    constexpr Rect(Point origin, Point size) noexcept
        : x(origin.x), y(origin.y), w(size.x), h(size.y) {}

    constexpr bool operator==(const Rect&) const noexcept = default;
    constexpr bool operator!=(const Rect&) const noexcept = default;

    constexpr double right() const noexcept { return x + w; }
    constexpr double bottom() const noexcept { return y + h; }
    constexpr Point origin() const noexcept { return Point{x, y}; }
    constexpr Point center() const noexcept { return Point{x + w / 2.0, y + h / 2.0}; }
    constexpr bool empty() const noexcept { return !(w > 0.0) || !(h > 0.0); }
    constexpr double area() const noexcept { return w * h; }

    constexpr bool contains(const Point& p) const noexcept {
        return p.x >= x && p.x < right() && p.y >= y && p.y < bottom();
    }
    constexpr bool contains(double px, double py) const noexcept {
        return contains(Point{px, py});
    }

    constexpr bool intersects(const Rect& o) const noexcept {
        return x < o.right() && o.x < right() && y < o.bottom() && o.y < bottom();
    }

    // 相交矩形；无交返回空矩形（w=h=0）。
    constexpr Rect intersection(const Rect& o) const noexcept {
        if (!intersects(o)) {
            return Rect{};
        }
        const double nx = x > o.x ? x : o.x;
        const double ny = y > o.y ? y : o.y;
        return Rect{nx, ny, (right() < o.right() ? right() : o.right()) - nx,
                    (bottom() < o.bottom() ? bottom() : o.bottom()) - ny};
    }

    // 包围盒；空矩形参与时返回另一方。
    constexpr Rect union_with(const Rect& o) const noexcept {
        if (empty()) {
            return o;
        }
        if (o.empty()) {
            return *this;
        }
        const double nx = x < o.x ? x : o.x;
        const double ny = y < o.y ? y : o.y;
        return Rect{nx, ny, (right() > o.right() ? right() : o.right()) - nx,
                    (bottom() > o.bottom() ? bottom() : o.bottom()) - ny};
    }
};

}  // namespace termchart

#endif  // TERMCHART_CORE_RECT_HPP
