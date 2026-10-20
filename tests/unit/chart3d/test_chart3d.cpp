// test_chart3d.cpp —— 3D 图表发射器：产出非空、结构正确的 scene::Scene
#include <doctest.h>

#include <cstdint>
#include <variant>
#include <vector>

#include "termchart/chart3d/projector.hpp"
#include "termchart/chart3d/scatter.hpp"
#include "termchart/chart3d/surface.hpp"
#include "termchart/math3d/camera.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {
using namespace termchart;
using namespace termchart::chart3d;
using namespace termchart::scene;

// 测试桩：把所有世界坐标钉到画布中心，用来验证注入的投影器真的被
// scatter3d / surface3d 走（而不是内部另算 Camera）。
class CenterStubProjector final : public IProjector {
public:
    [[nodiscard]] math3d::Projected project(const math3d::Vec3&, double vw,
                                            double vh) const noexcept override {
        return math3d::Projected{vw * 0.5, vh * 0.5, 0.5, true};
    }
};

TEST_CASE("chart3d/scatter: 点云产出非空 Scene 且含填充点") {
    const std::vector<math3d::Vec3> pts = {{0, 0, 0},   {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
                                           {1, 1, 1},   {-1, -1, -1}, {2, 2, 2}};
    const Scene s = scatter3d(pts);
    CHECK_FALSE(s.empty());
    std::size_t fills = 0, rects = 0, strokes = 0, moves = 0, lines = 0;
    for (const auto& c : s.commands()) {
        if (std::holds_alternative<FillOp>(c)) ++fills;
        if (std::holds_alternative<RectOp>(c)) ++rects;
        if (std::holds_alternative<StrokeOp>(c)) ++strokes;
        if (std::holds_alternative<MoveTo>(c)) ++moves;
        if (std::holds_alternative<LineTo>(c)) ++lines;
    }
    CHECK(fills >= 1);    // 至少有点被填充
    CHECK(rects >= 1);    // 每个点一个方块
    CHECK(strokes >= 1);  // 坐标轴
    CHECK(moves >= 1);
    CHECK(lines >= 1);
}

TEST_CASE("chart3d/scatter: 空输入返回空 Scene") {
    CHECK(scatter3d({}).empty());
}

TEST_CASE("chart3d/surface: 线框曲面产出大量线段") {
    const Scene s = surface3d([](double x, double y) { return std::sin(x) * std::cos(y); }, -3.0,
                             3.0, -3.0, 3.0);
    CHECK_FALSE(s.empty());
    std::size_t lines = 0, strokes = 0;
    for (const auto& c : s.commands()) {
        if (std::holds_alternative<LineTo>(c)) ++lines;
        if (std::holds_alternative<StrokeOp>(c)) ++strokes;
    }
    CHECK(lines > 20);  // 28×22 网格应有大量连线
    CHECK(strokes >= 1);
}

TEST_CASE("chart3d/projector: Projector3D 与 Camera 逐字段一致（封装不改变行为）") {
    const math3d::Camera cam =
        make_camera(72.0, 28.0, 35.0, 22.0, 55.0, math3d::Vec3{0.0, 0.0, 0.0}, 3.0);
    const Projector3D proj{cam};
    const math3d::Vec3 p{1.0, -0.5, 0.7};
    const auto via_camera = cam.project(p, 72.0, 28.0);
    const auto via_projector = proj.project(p, 72.0, 28.0);
    CHECK(via_camera.visible == via_projector.visible);
    CHECK(via_camera.x == doctest::Approx(via_projector.x));
    CHECK(via_camera.y == doctest::Approx(via_projector.y));
    CHECK(via_camera.depth == doctest::Approx(via_projector.depth));
}

TEST_CASE("chart3d/projector: scatter3d 走注入的投影器（缝生效）") {
    const CenterStubProjector stub;
    const std::vector<math3d::Vec3> pts = {{0, 0, 0}, {1, 1, 1}, {-1, 2, 0.5}};
    const Scene s = scatter3d(pts, Scatter3dOpts{}, &stub);
    std::size_t rects = 0;
    for (const auto& c : s.commands()) {
        if (const auto* r = std::get_if<RectOp>(&c)) {
            ++rects;
            // 默认 point_size=0.9 → h=0.45：每个点都被钉在画布中心 (36,14)。
            CHECK(r->rect.x == doctest::Approx(36.0 - 0.45));
            CHECK(r->rect.y == doctest::Approx(14.0 - 0.45));
        }
    }
    CHECK(rects == pts.size());  // 每个可见点一个方块，全部走注入投影器
}

TEST_CASE("chart3d/projector: surface3d 走注入的投影器（缝生效）") {
    const CenterStubProjector stub;
    Surface3dOpts o;
    o.res_x = 3;
    o.res_y = 3;
    const Scene s = surface3d([](double x, double y) { return x + y; }, -1.0, 1.0, -1.0, 1.0, o,
                              &stub);
    CHECK_FALSE(s.empty());
    for (const auto& c : s.commands()) {
        if (const auto* m = std::get_if<MoveTo>(&c)) {
            CHECK(m->to.x == doctest::Approx(36.0));  // 线段端点全被钉在中心
            CHECK(m->to.y == doctest::Approx(14.0));
        }
        if (const auto* l = std::get_if<LineTo>(&c)) {
            CHECK(l->to.x == doctest::Approx(36.0));
            CHECK(l->to.y == doctest::Approx(14.0));
        }
    }
}

TEST_CASE("chart3d/projector: 默认路径与注入路径产出不同（证明确实分了叉）") {
    const std::vector<math3d::Vec3> pts = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const Scene default_scene = scatter3d(pts);
    const CenterStubProjector stub;
    const Scene injected_scene = scatter3d(pts, Scatter3dOpts{}, &stub);
    // 默认路径按包围盒分散投影，注入路径全钉中心：两者命令序列必然不同。
    CHECK_FALSE(default_scene == injected_scene);
}
}  // namespace
