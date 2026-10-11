// transform.cpp —— 2D 仿射变换实现
#include "termchart/canvas/transform.hpp"

#include <cmath>
#include <vector>

namespace termchart::canvas {
namespace {

constexpr double kIdentityEps = 1e-12;

}  // namespace

Transform Transform::translation(double dx, double dy) noexcept {
    Transform out;
    out.m02_ = dx;
    out.m12_ = dy;
    return out;
}

Transform Transform::scaling(double sx, double sy) noexcept {
    Transform out;
    out.m00_ = sx;
    out.m11_ = sy;
    return out;
}

Transform Transform::rotation(double radians) noexcept {
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    Transform out;
    out.m00_ = c;
    out.m01_ = -s;
    out.m10_ = s;
    out.m11_ = c;
    return out;
}

Transform Transform::shearing(double shx, double shy) noexcept {
    Transform out;
    out.m01_ = shx;
    out.m10_ = shy;
    return out;
}

Transform Transform::rotation_around(Point center, double radians) noexcept {
    return translation(-center.x, -center.y).then(rotation(radians)).then(
        translation(center.x, center.y));
}

Transform Transform::aspect_correction(double cell_aspect) noexcept {
    // 字符格高是宽的 cell_aspect 倍：把 y 压掉 1/aspect，几何圆才会显圆。
    const double aspect = cell_aspect > 0.0 ? cell_aspect : 1.0;
    return scaling(1.0, 1.0 / aspect);
}

Transform Transform::then(const Transform& next) const noexcept {
    Transform out;
    out.m00_ = next.m00_ * m00_ + next.m01_ * m10_;
    out.m01_ = next.m00_ * m01_ + next.m01_ * m11_;
    out.m02_ = next.m00_ * m02_ + next.m01_ * m12_ + next.m02_;
    out.m10_ = next.m10_ * m00_ + next.m11_ * m10_;
    out.m11_ = next.m10_ * m01_ + next.m11_ * m11_;
    out.m12_ = next.m10_ * m02_ + next.m11_ * m12_ + next.m12_;
    return out;
}

double Transform::determinant() const noexcept {
    return m00_ * m11_ - m01_ * m10_;
}

bool Transform::invertible() const noexcept {
    return std::fabs(determinant()) > kIdentityEps;
}

Transform Transform::inverse() const noexcept {
    const double det = determinant();
    if (std::fabs(det) <= kIdentityEps) {
        return Transform{};  // 不可逆：返回单位变换
    }
    Transform out;
    const double inv = 1.0 / det;
    out.m00_ = m11_ * inv;
    out.m01_ = -m01_ * inv;
    out.m10_ = -m10_ * inv;
    out.m11_ = m00_ * inv;
    // 平移部分：-M⁻¹·t
    out.m02_ = -(out.m00_ * m02_ + out.m01_ * m12_);
    out.m12_ = -(out.m10_ * m02_ + out.m11_ * m12_);
    return out;
}

Point Transform::apply(Point p) const noexcept {
    return Point{m00_ * p.x + m01_ * p.y + m02_, m10_ * p.x + m11_ * p.y + m12_};
}

Rect Transform::apply_bounds(const Rect& rect) const noexcept {
    const Point corners[4] = {
        apply(Point{rect.x, rect.y}),
        apply(Point{rect.right(), rect.y}),
        apply(Point{rect.right(), rect.bottom()}),
        apply(Point{rect.x, rect.bottom()}),
    };
    double min_x = corners[0].x;
    double max_x = corners[0].x;
    double min_y = corners[0].y;
    double max_y = corners[0].y;
    for (const Point& p : corners) {
        min_x = p.x < min_x ? p.x : min_x;
        max_x = p.x > max_x ? p.x : max_x;
        min_y = p.y < min_y ? p.y : min_y;
        max_y = p.y > max_y ? p.y : max_y;
    }
    return Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

std::vector<Point> Transform::apply_all(const std::vector<Point>& points) const {
    std::vector<Point> out;
    out.reserve(points.size());
    for (const Point& p : points) {
        out.push_back(apply(p));
    }
    return out;
}

bool Transform::is_identity() const noexcept {
    return std::fabs(m00_ - 1.0) <= kIdentityEps && std::fabs(m01_) <= kIdentityEps &&
           std::fabs(m02_) <= kIdentityEps && std::fabs(m10_) <= kIdentityEps &&
           std::fabs(m11_ - 1.0) <= kIdentityEps && std::fabs(m12_) <= kIdentityEps;
}

}  // namespace termchart::canvas
