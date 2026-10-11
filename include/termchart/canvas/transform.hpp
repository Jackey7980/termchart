// transform.hpp —— 2D 仿射变换（含终端字符的宽高比补偿）
//
// 约定：列向量，p' = M · [x y 1]^T，元素按行主书写
//     | m00 m01 m02 |
//     | m10 m11 m12 |
//     |  0   0   1  |
// `then(next)` 表示"先做 this、再做 next"（等价于矩阵乘法 next·this），
// 因此链式写法与阅读顺序一致：T.translation(...).then(T.rotation(...)) 先平移后旋转。
//
// 宽高比补偿：终端字符格不是正方形，宽:高通常约 1:2。几何上的正圆直接落格会显扁，
// 所以画圆/放射布局时先用 aspect_correction() 把 y 方向压缩 1/aspect，
// 渲染出来才接近正圆。这也是本模块区别于通用线性代数的唯一一点。
#ifndef TERMCHART_CANVAS_TRANSFORM_HPP
#define TERMCHART_CANVAS_TRANSFORM_HPP

#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {

class Transform {
public:
    Transform() = default;  // 单位变换

    [[nodiscard]] static Transform translation(double dx, double dy) noexcept;
    [[nodiscard]] static Transform scaling(double sx, double sy) noexcept;
    [[nodiscard]] static Transform rotation(double radians) noexcept;
    [[nodiscard]] static Transform shearing(double shx, double shy) noexcept;
    // 绕给定中心旋转。
    [[nodiscard]] static Transform rotation_around(Point center, double radians) noexcept;
    // 供"字符网格上的圆/放射布局"用：y 方向压缩 1/cell_aspect。
    [[nodiscard]] static Transform aspect_correction(double cell_aspect = 2.0) noexcept;

    // 先本变换、后 next。
    [[nodiscard]] TERMCHART_API Transform then(const Transform& next) const noexcept;
    [[nodiscard]] TERMCHART_API double determinant() const noexcept;
    [[nodiscard]] TERMCHART_API bool invertible() const noexcept;
    // 不可逆（行列式为 0）时返回单位变换；调用方若需要区分，先查 invertible()。
    [[nodiscard]] TERMCHART_API Transform inverse() const noexcept;

    [[nodiscard]] TERMCHART_API Point apply(Point p) const noexcept;
    // 变换后的**轴对齐包围盒**：四个角分别变换后取并集（含负缩放情形）。
    [[nodiscard]] TERMCHART_API Rect apply_bounds(const Rect& rect) const noexcept;
    [[nodiscard]] TERMCHART_API std::vector<Point> apply_all(
        const std::vector<Point>& points) const;

    [[nodiscard]] double m00() const noexcept { return m00_; }
    [[nodiscard]] double m01() const noexcept { return m01_; }
    [[nodiscard]] double m02() const noexcept { return m02_; }
    [[nodiscard]] double m10() const noexcept { return m10_; }
    [[nodiscard]] double m11() const noexcept { return m11_; }
    [[nodiscard]] double m12() const noexcept { return m12_; }

    [[nodiscard]] bool is_identity() const noexcept;

private:
    double m00_ = 1.0;
    double m01_ = 0.0;
    double m02_ = 0.0;
    double m10_ = 0.0;
    double m11_ = 1.0;
    double m12_ = 0.0;
};

}  // namespace termchart::canvas

#endif  // TERMCHART_CANVAS_TRANSFORM_HPP
