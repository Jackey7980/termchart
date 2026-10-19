// test_chart3d.cpp —— 3D 图表发射器：产出非空、结构正确的 scene::Scene
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "termchart/chart3d/scatter.hpp"
#include "termchart/chart3d/surface.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {
using namespace termchart;
using namespace termchart::chart3d;
using namespace termchart::scene;

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
}  // namespace
