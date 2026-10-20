// vec4.hpp —— 四维向量（header-only）
//
// math3d::Vec3 的 4D 对应物。仅放 4D 投影真正需要的运算；依赖 <cmath>，
// 不引入 eigen/glm。约定与 vec3.hpp 一致：行主序矩阵 × 列向量（见 mat5.hpp），
// 本文件只负责向量代数。4D 没有叉积的直推（那是 7D 才有），故不提供 cross。
#ifndef TERMCHART_MATH4D_VEC4_HPP
#define TERMCHART_MATH4D_VEC4_HPP

#include <cmath>

namespace termchart::math4d {

struct Vec4 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 0.0;

    constexpr Vec4() noexcept = default;
    constexpr Vec4(double x_, double y_, double z_, double w_) noexcept
        : x(x_), y(y_), z(z_), w(w_) {}

    constexpr Vec4 operator+(const Vec4& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z, w + o.w};
    }
    constexpr Vec4 operator-(const Vec4& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z, w - o.w};
    }
    constexpr Vec4 operator-() const noexcept { return {-x, -y, -z, -w}; }
    constexpr Vec4 operator*(double s) const noexcept { return {x * s, y * s, z * s, w * s}; }
    constexpr Vec4 operator/(double s) const noexcept { return {x / s, y / s, z / s, w / s}; }

    constexpr double dot(const Vec4& o) const noexcept {
        return x * o.x + y * o.y + z * o.z + w * o.w;
    }
    constexpr double length() const noexcept { return std::sqrt(x * x + y * y + z * z + w * w); }
    constexpr Vec4 normalized() const noexcept {
        const double l = length();
        return l > 0.0 ? (*this) / l : Vec4{};
    }
    constexpr bool operator==(const Vec4& o) const noexcept {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }
};

}  // namespace termchart::math4d

#endif  // TERMCHART_MATH4D_VEC4_HPP
