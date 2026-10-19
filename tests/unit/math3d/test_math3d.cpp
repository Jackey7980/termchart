// test_math3d.cpp —— 3D 投影数学（vec3 / mat4 / camera）确定性单测
#include <doctest.h>

#include <cmath>

#include "termchart/math3d/camera.hpp"
#include "termchart/math3d/mat4.hpp"
#include "termchart/math3d/vec3.hpp"

namespace {
using namespace termchart::math3d;

constexpr double kPi = 3.14159265358979323846;

bool close(double a, double b, double eps = 1e-9) noexcept {
    return std::fabs(a - b) <= eps;
}

TEST_CASE("math3d/vec3: 基本代数与叉乘") {
    const Vec3 a{1, 2, 3}, b{4, 5, 6};
    CHECK((a + b) == Vec3{5, 7, 9});
    CHECK((a - b) == Vec3{-3, -3, -3});
    CHECK((a * 2) == Vec3{2, 4, 6});
    CHECK(close(a.dot(b), 32.0));                  // 1*4+2*5+3*6
    CHECK((a.cross(b)) == Vec3{-3, 6, -3});
    CHECK(close(a.length(), std::sqrt(14.0)));
    const Vec3 n = a.normalized();
    CHECK(close(n.length(), 1.0));
}

TEST_CASE("math3d/mat4: 单位阵与平移") {
    const Mat4 I;  // 默认即单位阵
    CHECK(I.transform(Vec3{2, 3, 4}) == Vec3{2, 3, 4});
    const Mat4 T = translate(1, 2, 3);
    CHECK(T.transform(Vec3{0, 0, 0}) == Vec3{1, 2, 3});
    CHECK(T.transform(Vec3{5, 0, 0}) == Vec3{6, 2, 3});
}

TEST_CASE("math3d/mat4: 缩放与坐标旋转") {
    const Mat4 S = scale(2, 3, 4);
    CHECK(S.transform(Vec3{1, 1, 1}) == Vec3{2, 3, 4});
    const Vec3 rx = rotate_x(kPi / 2).transform(Vec3{0, 1, 0});
    CHECK((close(rx.x, 0.0) && close(rx.y, 0.0) && close(rx.z, 1.0)));
    const Vec3 ry = rotate_y(kPi / 2).transform(Vec3{1, 0, 0});
    CHECK((close(ry.x, 0.0) && close(ry.y, 0.0) && close(ry.z, -1.0)));
}

TEST_CASE("math3d/mat4: 乘法 = 变换链") {
    const Mat4 C = translate(1, 0, 0) * scale(2, 2, 2);
    // 先缩放后平移：(1,1,1)→(2,2,2)→(3,2,2)
    CHECK(C.transform(Vec3{1, 1, 1}) == Vec3{3, 2, 2});
}

TEST_CASE("math3d/camera: 原点投影到画布中心且为近处") {
    Camera cam;
    cam.eye = Vec3{0, 0, 5};
    cam.target = Vec3{0, 0, 0};
    cam.up = Vec3{0, 1, 0};
    cam.fovy_deg = 60.0;
    cam.aspect = 1.0;
    cam.near = 0.1;
    cam.far = 100.0;
    const auto p = cam.project(Vec3{0, 0, 0}, 80.0, 40.0);
    REQUIRE(p.visible);
    CHECK(close(p.x, 40.0, 1e-6));
    CHECK(close(p.y, 20.0, 1e-6));
    CHECK((p.depth > 0.0 && p.depth < 0.2));  // 靠近相机
}

TEST_CASE("math3d/camera: 相机后方点不可见") {
    Camera cam;
    cam.eye = Vec3{0, 0, 5};
    cam.target = Vec3{0, 0, 0};
    const auto p = cam.project(Vec3{0, 0, 10}, 80.0, 40.0);  // 在 eye 背后
    CHECK_FALSE(p.visible);
}

TEST_CASE("math3d/camera: orbit 放置 eye 于目标上方") {
    Camera cam;
    cam.target = Vec3{1, 2, 3};
    cam.orbit(0.0, 90.0, 5.0);  // 仰角 90° → 正上方
    CHECK(close(cam.eye.x, 1.0, 1e-9));
    CHECK(close(cam.eye.z, 3.0, 1e-9));
    CHECK(cam.eye.y > 3.0);  // 高于目标
}
}  // namespace
