// shape.cpp —— 形状轮廓生成与绘制
#include "termchart/graph/shape/shape.hpp"

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "termchart/canvas/canvas.hpp"

namespace termchart::graph {

namespace {

using CellChar = std::pair<Point, char32_t>;

// 直角矩形：U+250C / U+2510 / U+2514 / U+2518 与 U+2500 / U+2502。
void emit_box(std::vector<CellChar>& out, Rect box, bool rounded) {
    const double x0 = box.x;
    const double y0 = box.y;
    const double x1 = box.right() - 1.0;
    const double y1 = box.bottom() - 1.0;
    if (box.w < 1.0 || box.h < 1.0) {
        return;
    }
    if (box.w == 1.0 && box.h == 1.0) {
        out.emplace_back(Point{x0, y0}, U'\u2500');
        return;
    }

    const char32_t top_left = rounded ? U'\u256D' : U'\u250C';
    const char32_t top_right = rounded ? U'\u256E' : U'\u2510';
    const char32_t bottom_left = rounded ? U'\u2570' : U'\u2514';
    const char32_t bottom_right = rounded ? U'\u256F' : U'\u2518';

    out.emplace_back(Point{x0, y0}, top_left);
    out.emplace_back(Point{x1, y0}, top_right);
    out.emplace_back(Point{x0, y1}, bottom_left);
    out.emplace_back(Point{x1, y1}, bottom_right);
    for (double x = x0 + 1.0; x < x1; x += 1.0) {
        out.emplace_back(Point{x, y0}, U'\u2500');
        out.emplace_back(Point{x, y1}, U'\u2500');
    }
    for (double y = y0 + 1.0; y < y1; y += 1.0) {
        out.emplace_back(Point{x0, y}, U'\u2502');
        out.emplace_back(Point{x1, y}, U'\u2502');
    }
}

// 菱形：按行计算左右边界，上半用反斜杠/斜杠，下半相反。
void emit_diamond(std::vector<CellChar>& out, Rect box) {
    if (box.w < 1.0 || box.h < 1.0) {
        return;
    }
    const double cx = box.x + (box.w - 1.0) / 2.0;
    const double half = (box.w - 1.0) / 2.0;
    const int rows = static_cast<int>(std::lround(box.h));
    const double mid = (static_cast<double>(rows) - 1.0) / 2.0;

    for (int i = 0; i < rows; ++i) {
        const double t = (rows <= 1) ? 0.0 : (static_cast<double>(i) - mid) / mid;
        const double shrink = std::fabs(t);          // 0 在中间行，1 在上下顶点
        const double hw = half * (1.0 - shrink);
        const double y = box.y + static_cast<double>(i);
        const double left = std::round(cx - hw);
        const double right = std::round(cx + hw);
        const bool upper = static_cast<double>(i) < mid;

        const char32_t left_char = upper ? U'\u2571' : U'\u2572';
        const char32_t right_char = upper ? U'\u2572' : U'\u2571';
        if (left == right) {
            out.emplace_back(Point{left, y}, U'\u2571');
            continue;
        }
        out.emplace_back(Point{left, y}, left_char);
        out.emplace_back(Point{right, y}, right_char);
    }
}

}  // namespace

std::vector<Point> shape_outline(ShapeKind kind, Rect box) {
    std::vector<CellChar> cells;
    switch (kind) {
        case ShapeKind::Box:
            emit_box(cells, box, false);
            break;
        case ShapeKind::Rounded:
            emit_box(cells, box, true);
            break;
        case ShapeKind::Diamond:
            emit_diamond(cells, box);
            break;
    }
    std::vector<Point> points;
    points.reserve(cells.size());
    for (const CellChar& cell : cells) {
        points.push_back(cell.first);
    }
    return points;
}

void draw_shape(canvas::Canvas& canvas, ShapeKind kind, Rect box, const Style& style) {
    std::vector<CellChar> cells;
    switch (kind) {
        case ShapeKind::Box:
            emit_box(cells, box, false);
            break;
        case ShapeKind::Rounded:
            emit_box(cells, box, true);
            break;
        case ShapeKind::Diamond:
            emit_diamond(cells, box);
            break;
    }
    for (const CellChar& cell : cells) {
        canvas.put(cell.first, cell.second, style);
    }
}

}  // namespace termchart::graph
