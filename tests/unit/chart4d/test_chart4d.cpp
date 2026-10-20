// test_chart4d.cpp —— Projector4D 两阶段投影 + wire4d 线框发射
#include <doctest.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "termchart/chart3d/projector.hpp"  // chart3d::IProjector
#include "termchart/chart4d/projector4d.hpp"
#include "termchart/chart4d/wire.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/math4d/camera4.hpp"
#include "termchart/math4d/mat5.hpp"
#include "termchart/math4d/vec4.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {
using namespace termchart;

// 固定桩：把任意 3D 点钉到画布中心，用来验证两阶段确实走到了内层投影器。
class CenterStub final : public chart3d::IProjector {
public:
    [[nodiscard]] math3d::Projected project(const math3d::Vec3&, double vw,
                                            double vh) const noexcept override {
        return math3d::Projected{vw * 0.5, vh * 0.5, 0.25, true};
    }
};

constexpr double kPiOver2 = 1.57079632679489661923;

// 记录型桩：捕获阶段二收到的 3D 点，供"阶段一输出正确性"断言。
class RecorderStub final : public chart3d::IProjector {
public:
    mutable math3d::Vec3 last{};

    [[nodiscard]] math3d::Projected project(const math3d::Vec3& world, double vw,
                                            double vh) const noexcept override {
        last = world;
        return math3d::Projected{vw * 0.5, vh * 0.5, 0.5, true};
    }
};

// 16 顶点超立方体（4 坐标各取 ±1）。
std::vector<math4d::Vec4> hypercube_vertices() {
    std::vector<math4d::Vec4> v;
    v.reserve(16);
    for (int i = 0; i < 16; ++i) {
        v.push_back(math4d::Vec4{(i & 1) ? 1.0 : -1.0, (i & 2) ? 1.0 : -1.0,
                                 (i & 4) ? 1.0 : -1.0, (i & 8) ? 1.0 : -1.0});
    }
    return v;
}

// 32 条边：仅连二进制位相差 1 的顶点对（4-正则，16×4/2=32）。
std::vector<chart4d::Edge> hypercube_edges() {
    std::vector<chart4d::Edge> e;
    for (int i = 0; i < 16; ++i) {
        for (int b = 0; b < 4; ++b) {
            const int j = i ^ (1 << b);
            if (j > i) {
                e.push_back({static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j)});
            }
        }
    }
    return e;
}

TEST_CASE("chart4d/hypercube: 顶点 16、边 32") {
    CHECK(hypercube_vertices().size() == 16);
    CHECK(hypercube_edges().size() == 32);
}

TEST_CASE("chart4d/projector4d: 两阶段= 4D 阶段后交内层投影器") {
    auto inner = std::make_shared<CenterStub>();
    math4d::Camera4 cam4;
    cam4.distance = 3.0;
    cam4.focal = 2.0;
    cam4.near = 0.1;
    const chart4d::Projector4D proj(math4d::Mat5{}, cam4, inner);  // 单位旋转

    // 手动两阶段：单位旋转不改点；camera4 把 (2,0,0,0) 投成 (4/3,0,0)；内层桩
    // 忽略具体 3D 值、一律返回画布中心。故 project 结果应等于桩输出。
    const auto got = proj.project(math4d::Vec4{2, 0, 0, 0}, 80.0, 30.0);
    CHECK(got.visible);
    CHECK(got.x == doctest::Approx(40.0));  // 80*0.5
    CHECK(got.y == doctest::Approx(15.0));  // 30*0.5
    CHECK(got.depth == doctest::Approx(0.25));
}

TEST_CASE("chart4d/projector4d: 4D 相机后方的点不可见（不调内层）") {
    auto inner = std::make_shared<CenterStub>();
    math4d::Camera4 cam4;
    cam4.distance = 1.0;
    cam4.focal = 2.0;
    cam4.near = 0.1;
    const chart4d::Projector4D proj(math4d::Mat5{}, cam4, inner);
    // w=1 → w_view=0（相机处）→ 阶段一即判不可见，内层永不被调用。
    CHECK_FALSE(proj.project(math4d::Vec4{0, 0, 0, 1.0}, 80.0, 30.0).visible);
    CHECK_FALSE(proj.project(math4d::Vec4{1, 1, 1, 5.0}, 80.0, 30.0).visible);  // 远处后方
}

TEST_CASE("chart4d/projector4d: Vec3 入口转发到内层（满足 IProjector）") {
    auto inner = std::make_shared<CenterStub>();
    const chart4d::Projector4D proj(math4d::Mat5{}, math4d::Camera4{}, inner);
    const chart3d::IProjector& as_base = proj;  // 经基类引用走虚分发
    const auto got = as_base.project(math3d::Vec3{1, 2, 3}, 80.0, 30.0);
    CHECK(got.visible);
    CHECK(got.x == doctest::Approx(40.0));  // 内层桩中心，4D 阶段未参与
    CHECK(got.y == doctest::Approx(15.0));
}

TEST_CASE("chart4d/projector4d: 4D 旋转确实改变投影（XW 90° 把 x 搬去 w）") {
    auto inner = std::make_shared<CenterStub>();
    math4d::Camera4 cam4;
    cam4.distance = 3.0;
    cam4.focal = 2.0;
    cam4.near = 0.1;
    // 记录型内层：捕获阶段二收到的 3D 点，验证阶段一输出。
    auto recorder = std::make_shared<RecorderStub>();
    const chart4d::Projector4D proj(math4d::rotate_xw(kPiOver2), cam4, recorder);
    // 点 (1,0,0,0) 绕 XW 转 90° → (0,0,0,1)：x 分量搬到 w，3D 部分归零。
    const auto got = proj.project(math4d::Vec4{1, 0, 0, 0}, 80.0, 30.0);
    CHECK(got.visible);
    CHECK(recorder->last.x == doctest::Approx(0.0));
    CHECK(recorder->last.y == doctest::Approx(0.0));
    CHECK(recorder->last.z == doctest::Approx(0.0));
    // 旋转后 w=1 → w_view=3-1=2, k=1；xyz 本为 0，故仍为 0。
}

TEST_CASE("chart4d/wire4d: 超立方体线框 Scene 结构（全可见）") {
    auto inner = std::make_shared<CenterStub>();
    math4d::Camera4 cam4;
    cam4.distance = 3.0;
    cam4.focal = 2.0;
    cam4.near = 0.1;
    const chart4d::Projector4D proj(math4d::Mat5{}, cam4, inner);

    const auto verts = hypercube_vertices();
    const auto edges = hypercube_edges();
    chart4d::Wire4dOpts o;
    o.cols = 80.0;
    o.rows = 30.0;
    const scene::Scene s = chart4d::wire4d(verts, edges, o, proj);

    // 桩令所有 4D 点经阶段一后可见（w∈[-1,1], w_view∈[2,4]>near），故：
    // 32 边 → 32 StrokeOp + 32 MoveTo + 32 LineTo；16 顶点 → 16 FillOp + 16 RectOp。
    std::size_t stroke = 0, move = 0, line = 0, fill = 0, rect = 0;
    for (const auto& c : s.commands()) {
        if (std::holds_alternative<scene::StrokeOp>(c)) ++stroke;
        if (std::holds_alternative<scene::MoveTo>(c)) ++move;
        if (std::holds_alternative<scene::LineTo>(c)) ++line;
        if (std::holds_alternative<scene::FillOp>(c)) ++fill;
        if (std::holds_alternative<scene::RectOp>(c)) ++rect;
    }
    CHECK(stroke == 32);
    CHECK(move == 32);
    CHECK(line == 32);
    CHECK(fill == 16);
    CHECK(rect == 16);
}

TEST_CASE("chart4d/wire4d: draw_vertices=false 时不出顶点图元") {
    auto inner = std::make_shared<CenterStub>();
    const chart4d::Projector4D proj(math4d::Mat5{}, math4d::Camera4{}, inner);
    chart4d::Wire4dOpts o;
    o.draw_vertices = false;
    const scene::Scene s = chart4d::wire4d(hypercube_vertices(), hypercube_edges(), o, proj);
    std::size_t rect = 0, line = 0;
    for (const auto& c : s.commands()) {
        if (std::holds_alternative<scene::RectOp>(c)) ++rect;
        if (std::holds_alternative<scene::LineTo>(c)) ++line;
    }
    CHECK(rect == 0);
    CHECK(line == 32);  // 边仍在
}

}  // namespace
