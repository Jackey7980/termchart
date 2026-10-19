// mat4.hpp —— 4×4 矩阵（header-only，行主序）
//
// 约定（与绝大多数图形教材一致）：矩阵 × 列向量。元素 m[r*4+c] 即第 r 行第 c 列。
// `transform(p)` 计算 M·p（齐次 w=1）。投影链 = perspective · look_at，即
// clip = perspective(look_at(p))。所有函数 constexpr，便于单测断言确定性结果。
#ifndef TERMCHART_MATH3D_MAT4_HPP
#define TERMCHART_MATH3D_MAT4_HPP

#include "termchart/math3d/vec3.hpp"

#include <cmath>

namespace termchart::math3d {

struct Mat4 {
    double m[16] = {};  // 行主序，初值全 0

    constexpr Mat4() { identity(); }

    constexpr void identity() noexcept {
        for (int i = 0; i < 16; ++i) {
            m[i] = (i % 5 == 0) ? 1.0 : 0.0;  // 对角线（index 0,5,10,15）
        }
    }

    constexpr double& at(int r, int c) noexcept { return m[r * 4 + c]; }
    constexpr double at(int r, int c) const noexcept { return m[r * 4 + c]; }

    // M · (x,y,z,1)
    constexpr Vec3 transform(const Vec3& p) const noexcept {
        const double x = p.x, y = p.y, z = p.z, w = 1.0;
        return {m[0] * x + m[1] * y + m[2] * z + m[3] * w,
                m[4] * x + m[5] * y + m[6] * z + m[7] * w,
                m[8] * x + m[9] * y + m[10] * z + m[11] * w};
    }

    // 透视除法后的齐次 w（投影矩阵填在 m[11]，clip.w = -(view.z)）。
    constexpr double clip_w(const Vec3& p) const noexcept {
        const double x = p.x, y = p.y, z = p.z, w = 1.0;
        return m[12] * x + m[13] * y + m[14] * z + m[15] * w;
    }
};

constexpr Mat4 operator*(const Mat4& a, const Mat4& b) noexcept {
    Mat4 r;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            double s = 0.0;
            for (int k = 0; k < 4; ++k) {
                s += a.at(i, k) * b.at(k, j);
            }
            r.at(i, j) = s;
        }
    }
    return r;
}

constexpr Mat4 translate(double tx, double ty, double tz) noexcept {
    Mat4 r;
    r.at(0, 3) = tx;  // 平移量在最后一列（行主序下为 m[3]/m[7]/m[11]）
    r.at(1, 3) = ty;
    r.at(2, 3) = tz;
    return r;
}

constexpr Mat4 scale(double sx, double sy, double sz) noexcept {
    Mat4 r;
    r.at(0, 0) = sx;
    r.at(1, 1) = sy;
    r.at(2, 2) = sz;
    return r;
}

// 绕 X 轴旋转（右手系，正角逆时针看向 +X）。
constexpr Mat4 rotate_x(double a) noexcept {
    Mat4 r;
    const double c = std::cos(a), s = std::sin(a);
    r.at(1, 1) = c;
    r.at(1, 2) = -s;
    r.at(2, 1) = s;
    r.at(2, 2) = c;
    return r;
}

constexpr Mat4 rotate_y(double a) noexcept {
    Mat4 r;
    const double c = std::cos(a), s = std::sin(a);
    r.at(0, 0) = c;
    r.at(0, 2) = s;
    r.at(2, 0) = -s;
    r.at(2, 2) = c;
    return r;
}

constexpr Mat4 rotate_z(double a) noexcept {
    Mat4 r;
    const double c = std::cos(a), s = std::sin(a);
    r.at(0, 0) = c;
    r.at(0, 1) = -s;
    r.at(1, 0) = s;
    r.at(1, 1) = c;
    return r;
}

// 透视投影（右手系，相机看向 -Z）。fovy/near/far 单位为同一坐标空间。
inline Mat4 perspective(double fovy_rad, double aspect, double near, double far) noexcept {
    Mat4 r;  // 全 0
    const double f = 1.0 / std::tan(fovy_rad / 2.0);
    r.at(0, 0) = f / aspect;
    r.at(1, 1) = f;
    r.at(2, 2) = (far + near) / (near - far);
    r.at(2, 3) = (2.0 * far * near) / (near - far);
    r.at(3, 2) = -1.0;
    return r;
}

// 视图矩阵：把世界坐标变换到以 eye 为原点、看向 center 的相机空间。
// up 与视线近似平行时退化为 (0,0,1)，避免叉乘退化出零向量。
inline Mat4 look_at(const Vec3& eye, const Vec3& center, const Vec3& up) noexcept {
    const Vec3 z = (eye - center).normalized();
    Vec3 up_used = up;
    if (std::fabs(z.dot(up_used)) > 0.999) {
        up_used = Vec3{0.0, 0.0, 1.0};
    }
    const Vec3 x = up_used.cross(z).normalized();
    const Vec3 y = z.cross(x);
    Mat4 r;
    r.at(0, 0) = x.x;
    r.at(0, 1) = x.y;
    r.at(0, 2) = x.z;
    r.at(0, 3) = -x.dot(eye);
    r.at(1, 0) = y.x;
    r.at(1, 1) = y.y;
    r.at(1, 2) = y.z;
    r.at(1, 3) = -y.dot(eye);
    r.at(2, 0) = z.x;
    r.at(2, 1) = z.y;
    r.at(2, 2) = z.z;
    r.at(2, 3) = -z.dot(eye);
    r.at(3, 3) = 1.0;
    return r;
}

}  // namespace termchart::math3d

#endif  // TERMCHART_MATH3D_MAT4_HPP
