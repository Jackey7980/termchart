// test_graph_shapes.cpp —— 节点形状轮廓（含曲线形状的格粒度边界）
#include <doctest.h>

#include <cstddef>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/graph/shape/shape.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

const ShapeKind kAllKinds[] = {ShapeKind::Box,    ShapeKind::Rounded, ShapeKind::Diamond,
                               ShapeKind::Ellipse, ShapeKind::Stadium,
                               ShapeKind::Hexagon, ShapeKind::Cylinder};

// 轮廓里是否包含某个格。
bool has_cell(const std::vector<Point>& outline, double x, double y) {
    for (const Point& p : outline) {
        if (p.x == x && p.y == y) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("graph/shape: 所有带边框的形状都落在自己的盒子里") {
    const Rect box{2.0, 3.0, 12.0, 8.0};
    for (const ShapeKind kind : kAllKinds) {
        const std::vector<Point> outline = shape_outline(kind, box);
        CHECK_FALSE(outline.empty());
        for (const Point& p : outline) {
            CHECK(p.x >= box.x);
            CHECK(p.x <= box.right() - 1.0);
            CHECK(p.y >= box.y);
            CHECK(p.y <= box.bottom() - 1.0);
        }
    }
}

TEST_CASE("graph/shape: 矩形与圆角的四角字符保持原样（回归）") {
    const Rect box{0.0, 0.0, 4.0, 3.0};
    const std::vector<Point> rect = shape_outline(ShapeKind::Box, box);
    CHECK(has_cell(rect, 0.0, 0.0));
    CHECK(has_cell(rect, 3.0, 0.0));
    CHECK(has_cell(rect, 0.0, 2.0));
    CHECK(has_cell(rect, 3.0, 2.0));

    canvas::Canvas canvas(6, 4);
    draw_shape(canvas, ShapeKind::Box, box, Style{});
    CHECK(canvas.at(0, 0).cp == U'\u250C');
    draw_shape(canvas, ShapeKind::Rounded, box, Style{});
    CHECK(canvas.at(0, 0).cp == U'\u256D');
}

TEST_CASE("graph/shape: 椭圆上下收成一个点，中间行到达左右边界") {
    const Rect box{0.0, 0.0, 11.0, 7.0};  // 奇数宽高：有唯一的中心行
    const std::vector<Point> outline = shape_outline(ShapeKind::Ellipse, box);
    REQUIRE_FALSE(outline.empty());

    // 顶点与底点各只有一个格，且落在中列
    std::size_t top_row = 0;
    std::size_t bottom_row = 0;
    for (const Point& p : outline) {
        if (p.y == 0.0) {
            ++top_row;
        }
        if (p.y == 6.0) {
            ++bottom_row;
        }
    }
    CHECK(top_row == 1);
    CHECK(bottom_row == 1);
    CHECK(has_cell(outline, 5.0, 0.0));

    // 中间行到达左右边界
    CHECK(has_cell(outline, 0.0, 3.0));
    CHECK(has_cell(outline, 10.0, 3.0));

    // 比同尺寸矩形"瘦"：轮廓格数明显少于矩形周长
    CHECK(outline.size() < shape_outline(ShapeKind::Box, box).size());
}

TEST_CASE("graph/shape: 胶囊两端内收，直边上下对齐") {
    const Rect box{0.0, 0.0, 12.0, 7.0};
    const std::vector<Point> outline = shape_outline(ShapeKind::Stadium, box);

    // 中间几行用满宽
    CHECK(has_cell(outline, 0.0, 3.0));
    CHECK(has_cell(outline, 11.0, 3.0));
    // 顶行内收（左端不再贴边）
    CHECK_FALSE(has_cell(outline, 0.0, 0.0));

    canvas::Canvas canvas(14, 9);
    draw_shape(canvas, ShapeKind::Stadium, box, Style{});
    CHECK(canvas.at(0, 3).cp == U'\u2502');  // 直边是竖线
}

TEST_CASE("graph/shape: 六边形的四个顶点位置") {
    const Rect box{0.0, 0.0, 12.0, 7.0};
    const std::vector<Point> outline = shape_outline(ShapeKind::Hexagon, box);

    const double shoulder = 3.0;  // (12-1)/4 取整
    CHECK(has_cell(outline, 0.0, 3.0));                    // 左尖
    CHECK(has_cell(outline, 11.0, 3.0));                   // 右尖
    CHECK(has_cell(outline, shoulder, 0.0));               // 左上
    CHECK(has_cell(outline, 11.0 - shoulder, 0.0));        // 右上
    CHECK(has_cell(outline, shoulder, 6.0));               // 左下
    CHECK(has_cell(outline, 11.0 - shoulder, 6.0));        // 右下
    // 左尖与左上之间是斜线，不是竖线
    CHECK(has_cell(outline, 1.0, 2.0));
}

TEST_CASE("graph/shape: 圆柱有顶盖、侧边与底弧") {
    const Rect box{0.0, 0.0, 10.0, 12.0};
    const std::vector<Point> outline = shape_outline(ShapeKind::Cylinder, box);

    std::size_t top_row_cells = 0;
    std::size_t second_row_cells = 0;
    for (const Point& p : outline) {
        if (p.y == 0.0) {
            ++top_row_cells;
        }
        if (p.y == 1.0) {
            ++second_row_cells;
        }
    }
    CHECK(top_row_cells == 1);      // 椭圆顶点只有一个格
    CHECK(second_row_cells > 1);    // 顶盖向下变宽
    CHECK(has_cell(outline, 0.0, 6.0));   // 左侧边
    CHECK(has_cell(outline, 9.0, 6.0));   // 右侧边
    CHECK(has_cell(outline, 5.0, 11.0));  // 底弧最低点

    canvas::Canvas canvas(12, 14);
    draw_shape(canvas, ShapeKind::Cylinder, box, Style{});
    CHECK(canvas.at(0, 6).cp == U'\u2502');
}

TEST_CASE("graph/shape: 太小的盒子优雅退化，Icon 不画边框") {
    // 六边形/胶囊/圆柱在放不下时退化成矩形，而不是画出残缺的形状
    const Rect tiny{0.0, 0.0, 3.0, 2.0};
    CHECK(shape_outline(ShapeKind::Hexagon, tiny).size() ==
          shape_outline(ShapeKind::Box, tiny).size());
    CHECK(shape_outline(ShapeKind::Stadium, tiny).size() ==
          shape_outline(ShapeKind::Rounded, tiny).size());
    CHECK(shape_outline(ShapeKind::Cylinder, tiny).size() ==
          shape_outline(ShapeKind::Box, tiny).size());

    // Icon：没有边框，调用方自己提供内容
    const Rect box{0.0, 0.0, 6.0, 4.0};
    CHECK(shape_outline(ShapeKind::Icon, box).empty());
    canvas::Canvas canvas(8, 6);
    draw_shape(canvas, ShapeKind::Icon, box, Style{});
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            CHECK(canvas.at(x, y).cp == U' ');
        }
    }
}

TEST_CASE("graph/shape: 零尺寸盒子不产出任何格") {
    for (const ShapeKind kind : kAllKinds) {
        CHECK(shape_outline(kind, Rect{0.0, 0.0, 0.0, 0.0}).empty());
        CHECK(shape_outline(kind, Rect{0.0, 0.0, 5.0, 0.0}).empty());
    }
}
