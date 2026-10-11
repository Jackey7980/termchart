// point.hpp —— 浮点坐标点（header-only）
//
// canvas/ 用浮点，"最后一像素/半格"才能对齐；core 层只提供 POD 与必要算子。
#ifndef TERMCHART_CORE_POINT_HPP
#define TERMCHART_CORE_POINT_HPP

namespace termchart {

struct Point {
    double x = 0.0;
    double y = 0.0;

    constexpr Point() noexcept = default;
    constexpr Point(double px, double py) noexcept : x(px), y(py) {}

    constexpr bool operator==(const Point&) const noexcept = default;
    constexpr bool operator!=(const Point&) const noexcept = default;

    constexpr Point operator+(const Point& o) const noexcept { return Point{x + o.x, y + o.y}; }
    constexpr Point operator-(const Point& o) const noexcept { return Point{x - o.x, y - o.y}; }
    constexpr Point operator*(double s) const noexcept { return Point{x * s, y * s}; }
    constexpr Point operator/(double s) const noexcept { return Point{x / s, y / s}; }
    constexpr Point& operator+=(const Point& o) noexcept {
        x += o.x;
        y += o.y;
        return *this;
    }
    constexpr Point& operator-=(const Point& o) noexcept {
        x -= o.x;
        y -= o.y;
        return *this;
    }
};

}  // namespace termchart

#endif  // TERMCHART_CORE_POINT_HPP
