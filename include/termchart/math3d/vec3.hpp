// vec3.hpp —— 三维向量（header-only）
//
// 仅放 3D 投影真正需要的那几个运算；依赖 <cmath>，不引入 eigen/glm。
// 约定：行主序矩阵 × 列向量（见 mat4.hpp），本文件只负责向量代数。
#ifndef TERMCHART_MATH3D_VEC3_HPP
#define TERMCHART_MATH3D_VEC3_HPP

#include <cmath>

namespace termchart::math3d {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vec3() noexcept = default;
    constexpr Vec3(double x_, double y_, double z_) noexcept : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator-() const noexcept { return {-x, -y, -z}; }
    constexpr Vec3 operator*(double s) const noexcept { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(double s) const noexcept { return {x / s, y / s, z / s}; }

    constexpr double dot(const Vec3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    constexpr Vec3 cross(const Vec3& o) const noexcept {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    constexpr double length() const noexcept { return std::sqrt(x * x + y * y + z * z); }
    constexpr Vec3 normalized() const noexcept {
        const double l = length();
        return l > 0.0 ? (*this) / l : Vec3{};
    }
    constexpr bool operator==(const Vec3& o) const noexcept {
        return x == o.x && y == o.y && z == o.z;
    }
};

}  // namespace termchart::math3d

#endif  // TERMCHART_MATH3D_VEC3_HPP
