// mat5.hpp —— 5×5 矩阵（header-only，行主序，4D 齐次坐标）
//
// 约定与 mat4.hpp 一致：矩阵 × 列向量。用 5×5（而非 4×4）是为了容纳 4D
// 齐次坐标 (x,y,z,w,1)：`transform(p)` 计算 M·(x,y,z,w,1) 并取前四个分量，
// 于是平移也落进同一个矩阵（与 mat4 的 w=1 齐次平移同思路）。所有函数
// constexpr，便于单测断言确定性结果。
//
// 4D 旋转与 3D 的本质区别：3D 只有 3 个坐标轴、旋转发生在**轴正交的平面**里
// （XY/XZ/YZ）；4D 有 4 个轴，任取两轴张成一个旋转平面，共 C(4,2)=6 个：
//   XY(0,1) XZ(0,2) XW(0,3) YZ(1,2) YW(1,3) ZW(2,3)。
// 每个都是在对应二维子平面上嵌一个标准 2D 旋转、其余对角线为 1。
#ifndef TERMCHART_MATH4D_MAT5_HPP
#define TERMCHART_MATH4D_MAT5_HPP

#include <cmath>

#include "termchart/math4d/vec4.hpp"

namespace termchart::math4d {

struct Mat5 {
    double m[25] = {};  // 行主序，初值全 0

    constexpr Mat5() { identity(); }

    constexpr void identity() noexcept {
        for (int i = 0; i < 25; ++i) {
            m[i] = (i % 6 == 0) ? 1.0 : 0.0;  // 对角线（index 0,6,12,18,24）
        }
    }

    constexpr double& at(int r, int c) noexcept { return m[r * 5 + c]; }
    constexpr double at(int r, int c) const noexcept { return m[r * 5 + c]; }

    // M · (x,y,z,w,1)，取前 4 行（第 5 行是齐次占位，恒为 (0,0,0,0,1)·= 1）。
    constexpr Vec4 transform(const Vec4& p) const noexcept {
        const double x = p.x, y = p.y, z = p.z, w = p.w, t = 1.0;
        return {m[0] * x + m[1] * y + m[2] * z + m[3] * w + m[4] * t,
                m[5] * x + m[6] * y + m[7] * z + m[8] * w + m[9] * t,
                m[10] * x + m[11] * y + m[12] * z + m[13] * w + m[14] * t,
                m[15] * x + m[16] * y + m[17] * z + m[18] * w + m[19] * t};
    }
};

constexpr Mat5 operator*(const Mat5& a, const Mat5& b) noexcept {
    Mat5 r;
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            double s = 0.0;
            for (int k = 0; k < 5; ++k) {
                s += a.at(i, k) * b.at(k, j);
            }
            r.at(i, j) = s;
        }
    }
    return r;
}

// 4D 平移：平移量落在最后一列（行主序下为 m[4]/m[9]/m[14]/m[19]）。
constexpr Mat5 translate(double tx, double ty, double tz, double tw) noexcept {
    Mat5 r;
    r.at(0, 4) = tx;
    r.at(1, 4) = ty;
    r.at(2, 4) = tz;
    r.at(3, 4) = tw;
    return r;
}

constexpr Mat5 scale(double sx, double sy, double sz, double sw) noexcept {
    Mat5 r;
    r.at(0, 0) = sx;
    r.at(1, 1) = sy;
    r.at(2, 2) = sz;
    r.at(3, 3) = sw;
    return r;
}

// 绕 (i,j) 坐标平面的旋转（i,j ∈ {0,1,2,3}，对应 x,y,z,w），符号约定与
// mat4::rotate_x 一致（plane(1,2) 即 rotate_x）：at(i,i)=c at(i,j)=-s
// at(j,i)=s at(j,j)=c。正角把第 i 轴转向第 j 轴。
constexpr Mat5 rotate_plane(int i, int j, double a) noexcept {
    Mat5 r;
    const double c = std::cos(a), s = std::sin(a);
    r.at(i, i) = c;
    r.at(i, j) = -s;
    r.at(j, i) = s;
    r.at(j, j) = c;
    return r;
}

// 4D 的 6 个旋转平面（见文件头注释）。rotate_xy(90°) 把 e_x 送到 e_y，余类推。
constexpr Mat5 rotate_xy(double a) noexcept { return rotate_plane(0, 1, a); }
constexpr Mat5 rotate_xz(double a) noexcept { return rotate_plane(0, 2, a); }
constexpr Mat5 rotate_xw(double a) noexcept { return rotate_plane(0, 3, a); }
constexpr Mat5 rotate_yz(double a) noexcept { return rotate_plane(1, 2, a); }
constexpr Mat5 rotate_yw(double a) noexcept { return rotate_plane(1, 3, a); }
constexpr Mat5 rotate_zw(double a) noexcept { return rotate_plane(2, 3, a); }

}  // namespace termchart::math4d

#endif  // TERMCHART_MATH4D_MAT5_HPP
