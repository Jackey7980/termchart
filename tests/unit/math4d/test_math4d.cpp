// test_math4d.cpp —— Vec4 / Mat5 / 6 旋转平面 / 4D→3D 透视投影
#include <doctest.h>

#include <cmath>

#include "termchart/math4d/camera4.hpp"
#include "termchart/math4d/mat5.hpp"
#include "termchart/math4d/vec4.hpp"

namespace {
using namespace termchart::math4d;  // Vec4 / Mat5 / translate / scale / rotate_* 自由函数

constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] bool neard(double a, double b) { return std::fabs(a - b) < 1e-9; }
[[nodiscard]] bool vnear(const Vec4& a, const Vec4& b) {
    return neard(a.x, b.x) && neard(a.y, b.y) && neard(a.z, b.z) && neard(a.w, b.w);
}
}  // namespace

TEST_CASE("math4d/vec4: 基本运算") {
    const Vec4 a{1, 2, 3, 4};
    const Vec4 b{4, 3, 2, 1};
    const Vec4 s = a + b;
    CHECK((s.x == 5 && s.y == 5 && s.z == 5 && s.w == 5));
    const Vec4 d = a - b;
    CHECK((d.x == -3 && d.y == -1 && d.z == 1 && d.w == 3));
    CHECK(neard(a.dot(b), 20.0));  // 1*4+2*3+3*2+4*1
    const Vec4 m = a * 2.0;
    CHECK((m.x == 2 && m.w == 8));
    const Vec4 q = a / 2.0;
    CHECK(q.w == 2);
    CHECK(neard(a.length(), std::sqrt(30.0)));
    CHECK(a == Vec4{1, 2, 3, 4});
    CHECK(a != b);
    CHECK((-a) == Vec4{-1, -2, -3, -4});
    const Vec4 n = Vec4{3, 0, 0, 4}.normalized();
    CHECK(neard(n.length(), 1.0));
    CHECK((neard(n.x, 0.6) && neard(n.w, 0.8)));
}

TEST_CASE("math4d/mat5: 单位阵恒等 + 齐次占位") {
    const Mat5 I;
    const Vec4 p{1, -2, 3, -4};
    CHECK(I.transform(p) == p);  // 精确：单位阵是精确值
    CHECK((neard(I.at(0, 0), 1.0) && neard(I.at(4, 4), 1.0)));
    CHECK(neard(I.at(0, 4), 0.0));  // 非对角为 0
}

TEST_CASE("math4d/mat5: 4D 平移与缩放走齐次最后一列/对角线") {
    const Vec4 p{1, 2, 3, 4};
    const Vec4 t = translate(1, 1, 1, 1).transform(p);
    CHECK(t == Vec4{2, 3, 4, 5});
    const Vec4 sc = scale(2, 2, 2, 2).transform(p);
    CHECK(sc == Vec4{2, 4, 6, 8});
}

TEST_CASE("math4d/mat5: 6 个旋转平面各把首轴送到次轴（90°）") {
    // rotate_plane(i,j) 正角把 e_i 转向 e_j；逐个钉住方向与符号。
    CHECK(vnear(rotate_xy(kPi / 2).transform(Vec4{1, 0, 0, 0}), Vec4{0, 1, 0, 0}));
    CHECK(vnear(rotate_xz(kPi / 2).transform(Vec4{1, 0, 0, 0}), Vec4{0, 0, 1, 0}));
    CHECK(vnear(rotate_xw(kPi / 2).transform(Vec4{1, 0, 0, 0}), Vec4{0, 0, 0, 1}));
    CHECK(vnear(rotate_yz(kPi / 2).transform(Vec4{0, 1, 0, 0}), Vec4{0, 0, 1, 0}));
    CHECK(vnear(rotate_yw(kPi / 2).transform(Vec4{0, 1, 0, 0}), Vec4{0, 0, 0, 1}));
    CHECK(vnear(rotate_zw(kPi / 2).transform(Vec4{0, 0, 1, 0}), Vec4{0, 0, 0, 1}));
    CHECK(vnear(rotate_xw(0.0).transform(Vec4{1, 2, 3, 4}), Vec4{1, 2, 3, 4}));  // 0° 恒等
}

TEST_CASE("math4d/mat5: 旋转保长度 + 合成=依次变换") {
    const Vec4 v{1, -1, 2, -2};
    const Mat5 r = rotate_xw(0.7) * rotate_yz(1.3);
    CHECK(neard(r.transform(v).length(), v.length()));  // 旋转是正交变换
    const Vec4 composed = r.transform(v);
    const Vec4 sequential = rotate_xw(0.7).transform(rotate_yz(1.3).transform(v));
    CHECK(vnear(composed, sequential));  // (A*B)·v == A·(B·v)
}

TEST_CASE("math4d/camera4: 透视除以 W（近大远小）") {
    Camera4 cam;
    cam.distance = 3.0;
    cam.focal = 2.0;
    cam.near = 0.1;
    // w=0 → w_view=3, k=2/3, x = 2*2/3 = 4/3
    const auto p = cam.project(Vec4{2, 0, 0, 0});
    CHECK(p.visible);
    CHECK(neard(p.xyz.x, 4.0 / 3.0));
    CHECK((neard(p.xyz.y, 0.0) && neard(p.xyz.z, 0.0)));
    CHECK(neard(p.w_view, 3.0));
    // w=1 → 更靠近相机（w_view=2）→ 同一 x 投影更大（k=1）
    const auto q = cam.project(Vec4{2, 0, 0, 1});
    CHECK(q.visible);
    CHECK(neard(q.xyz.x, 2.0));
    CHECK(q.xyz.x > p.xyz.x);  // 近大远小
}

TEST_CASE("math4d/camera4: 相机处/后方/近裁剪内不可见") {
    Camera4 cam;
    cam.distance = 1.0;
    cam.focal = 2.0;
    cam.near = 0.1;
    CHECK_FALSE(cam.project(Vec4{0, 0, 0, 1.0}).visible);   // w_view=0（相机处）
    CHECK_FALSE(cam.project(Vec4{0, 0, 0, 2.0}).visible);   // w_view=-1（后方）
    CHECK_FALSE(cam.project(Vec4{0, 0, 0, 0.95}).visible);  // w_view=0.05 < near
    CHECK(cam.project(Vec4{0, 0, 0, 0.5}).visible);         // w_view=0.5 > near
}

TEST_CASE("math4d/camera4: focal_from_fov_deg 与 3D 同式") {
    // f = 1/tan(fov/2)：60° → 1/tan30° = √3 ≈ 1.732；90° → 1。
    CHECK(neard(focal_from_fov_deg(60.0), std::sqrt(3.0)));
    CHECK(neard(focal_from_fov_deg(90.0), 1.0));
    CHECK(focal_from_fov_deg(30.0) > focal_from_fov_deg(60.0));  // 视野越小焦距越大
}
