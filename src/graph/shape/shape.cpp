// shape.cpp —— 形状轮廓生成与绘制
//
// 所有形状都走同一个入口 build_shape() 产出"格 + 字符"序列，shape_outline 只取格、
// draw_shape 负责落笔 —— 分派与轮廓逻辑因此只有一份，不会出现"两处 switch 渐渐不一致"。
#include "termchart/graph/shape/shape.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/primitives.hpp"

namespace termchart::graph {
namespace {

using CellChar = std::pair<Point, char32_t>;

constexpr char32_t kHorizontal = U'\u2500';   // ─
constexpr char32_t kVertical = U'\u2502';     // │
constexpr char32_t kUpperLeft = U'\u250C';    // ┌
constexpr char32_t kUpperRight = U'\u2510';   // ┐
constexpr char32_t kLowerLeft = U'\u2514';    // └
constexpr char32_t kLowerRight = U'\u2518';   // ┘
constexpr char32_t kRoundUL = U'\u256D';      // ╭
constexpr char32_t kRoundUR = U'\u256E';      // ╮
constexpr char32_t kRoundLL = U'\u2570';      // ╰
constexpr char32_t kRoundLR = U'\u256F';      // ╯
constexpr char32_t kDownRight = U'\u2572';    // ╲
constexpr char32_t kUpRight = U'\u2571';      // ╱

void emit(std::vector<CellChar>& out, double x, double y, char32_t ch) {
    out.emplace_back(Point{x, y}, ch);
}

// 直角 / 圆角矩形。
void emit_box(std::vector<CellChar>& out, Rect box, bool rounded) {
    if (box.w < 1.0 || box.h < 1.0) {
        return;
    }
    const double x0 = box.x;
    const double y0 = box.y;
    const double x1 = box.right() - 1.0;
    const double y1 = box.bottom() - 1.0;
    if (box.w == 1.0 && box.h == 1.0) {
        emit(out, x0, y0, kHorizontal);
        return;
    }

    emit(out, x0, y0, rounded ? kRoundUL : kUpperLeft);
    emit(out, x1, y0, rounded ? kRoundUR : kUpperRight);
    emit(out, x0, y1, rounded ? kRoundLL : kLowerLeft);
    emit(out, x1, y1, rounded ? kRoundLR : kLowerRight);
    for (double x = x0 + 1.0; x < x1; x += 1.0) {
        emit(out, x, y0, kHorizontal);
        emit(out, x, y1, kHorizontal);
    }
    for (double y = y0 + 1.0; y < y1; y += 1.0) {
        emit(out, x0, y, kVertical);
        emit(out, x1, y, kVertical);
    }
}

// 逐行左右边界：调用方算好该行的半宽，这里负责取整、成对发射，
// 并在"左右边界重合"时只发一个格（曲线的上下端点就是这么来的）。
void emit_row_span(std::vector<CellChar>& out, Rect box, int row, double half_width,
                   double cx, char32_t left_char, char32_t right_char,
                   char32_t single_char) {
    const double y = box.y + static_cast<double>(row);
    const double left = std::round(cx - half_width);
    const double right = std::round(cx + half_width);
    if (left == right) {
        emit(out, left, y, single_char);
        return;
    }
    emit(out, left, y, left_char);
    emit(out, right, y, right_char);
}

// 上下端用 ─ 收口，中间用斜线；行宽接近最大半宽时改用竖线（曲线已趋垂直）。
char32_t side_char(bool upper, bool left, bool near_full) {
    if (near_full) {
        return kVertical;
    }
    if (upper) {
        return left ? kUpRight : kDownRight;
    }
    return left ? kDownRight : kUpRight;
}

// 菱形与椭圆共用"逐行半宽"的骨架，只是半宽随行的变化规律不同。
void emit_row_based(std::vector<CellChar>& out, Rect box, bool ellipse) {
    if (box.w < 1.0 || box.h < 1.0) {
        return;
    }
    const double cx = box.x + (box.w - 1.0) / 2.0;
    const double half = (box.w - 1.0) / 2.0;
    const int rows = static_cast<int>(std::lround(box.h));
    const double mid = (static_cast<double>(rows) - 1.0) / 2.0;

    for (int i = 0; i < rows; ++i) {
        const double offset = (mid <= 0.0) ? 0.0 : (static_cast<double>(i) - mid) / mid;
        const double shrink = std::fabs(offset);  // 0 中间行，1 上下顶点
        const double half_width =
            ellipse ? half * std::sqrt(std::max(0.0, 1.0 - shrink * shrink))
                    : half * (1.0 - shrink);
        const bool upper = static_cast<double>(i) < mid;
        const bool near_full = half > 0.0 && half_width >= half - 0.5;
        emit_row_span(out, box, i, half_width, cx, side_char(upper, true, near_full),
                      side_char(upper, false, near_full), kHorizontal);
    }
}

// 胶囊：两端各一个半径 r 的半圆，中间是直边。
void emit_stadium(std::vector<CellChar>& out, Rect box) {
    if (box.w < 1.0 || box.h < 1.0) {
        return;
    }
    if (box.w <= 2.0 || box.h <= 2.0) {
        emit_box(out, box, true);  // 太小：退化成圆角矩形
        return;
    }
    const double x0 = box.x;
    const double x1 = box.right() - 1.0;
    const int rows = static_cast<int>(std::lround(box.h));
    const double mid = (static_cast<double>(rows) - 1.0) / 2.0;
    const double radius = mid;  // 高的一半即端部半径

    for (int i = 0; i < rows; ++i) {
        const double dy = std::fabs(static_cast<double>(i) - mid);
        const double inset = radius - std::sqrt(std::max(0.0, radius * radius - dy * dy));
        const double y = box.y + static_cast<double>(i);
        const double left = x0 + std::round(inset);
        const double right = x1 - std::round(inset);
        if (i == 0 || i == rows - 1) {
            if (left == right) {
                emit(out, left, y, kHorizontal);
            } else {
                emit(out, left, y, i == 0 ? kRoundUL : kRoundLL);
                emit(out, right, y, i == 0 ? kRoundUR : kRoundLR);
            }
            continue;
        }
        if (left == right) {
            emit(out, left, y, kHorizontal);
            continue;
        }
        emit(out, left, y, kVertical);
        emit(out, right, y, kVertical);
    }
}

// 六边形：左右两个尖角 + 上下两条水平边。
void emit_hexagon(std::vector<CellChar>& out, Rect box) {
    if (box.w < 3.0 || box.h < 3.0) {
        emit_box(out, box, false);  // 太小：退化成矩形
        return;
    }
    const double x0 = box.x;
    const double y0 = box.y;
    const double x1 = box.right() - 1.0;
    const double y1 = box.bottom() - 1.0;
    const double mid_y = (y0 + y1) / 2.0;
    const double shoulder = std::round((x1 - x0) / 4.0);

    const Point vertices[6] = {
        Point{x0, mid_y},           // 左尖
        Point{x0 + shoulder, y0},   // 左上
        Point{x1 - shoulder, y0},   // 右上
        Point{x1, mid_y},           // 右尖
        Point{x1 - shoulder, y1},   // 右下
        Point{x0 + shoulder, y1},   // 左下
    };
    for (int i = 0; i < 6; ++i) {
        const Point from = vertices[i];
        const Point to = vertices[(i + 1) % 6];
        const std::vector<Point> points = canvas::line_points(from, to);
        const bool horizontal = std::fabs(from.y - to.y) < 0.5;
        const bool going_down = to.y > from.y;
        for (const Point& p : points) {
            emit(out, p.x, p.y,
                 horizontal ? kHorizontal : (going_down ? kDownRight : kUpRight));
        }
    }
}

// 椭圆的上半或下半（用于圆柱顶盖与底弧）。
void emit_ellipse_half(std::vector<CellChar>& out, Rect box, bool top_half) {
    if (box.w < 1.0 || box.h < 1.0) {
        return;
    }
    const double cx = box.x + (box.w - 1.0) / 2.0;
    const double half = (box.w - 1.0) / 2.0;
    const int rows = static_cast<int>(std::lround(box.h));
    const int first = top_half ? 0 : rows / 2;
    const int last = top_half ? rows / 2 : rows - 1;
    const double mid = (static_cast<double>(rows) - 1.0) / 2.0;
    for (int i = first; i <= last; ++i) {
        const double offset = (mid <= 0.0) ? 0.0 : (static_cast<double>(i) - mid) / mid;
        const double shrink = std::fabs(offset);
        const double half_width = half * std::sqrt(std::max(0.0, 1.0 - shrink * shrink));
        const bool upper = top_half;
        const bool near_full = half > 0.0 && half_width >= half - 0.5;
        emit_row_span(out, box, i, half_width, cx, side_char(upper, true, near_full),
                      side_char(upper, false, near_full), kHorizontal);
    }
}

// 圆柱：顶盖椭圆 + 两侧直边 + 底部弧。
void emit_cylinder(std::vector<CellChar>& out, Rect box) {
    if (box.w < 3.0 || box.h < 4.0) {
        emit_box(out, box, false);  // 太小：退化成矩形
        return;
    }
    const int rows = static_cast<int>(std::lround(box.h));
    const int cap = std::max(2, rows / 3);
    emit_ellipse_half(out, Rect{box.x, box.y, box.w, static_cast<double>(cap) * 2.0}, true);
    emit_ellipse_half(out,
                      Rect{box.x, box.bottom() - static_cast<double>(cap) * 2.0, box.w,
                           static_cast<double>(cap) * 2.0},
                      false);
    const double x0 = box.x;
    const double x1 = box.right() - 1.0;
    const double y_top = box.y + static_cast<double>(cap) / 2.0;
    const double y_bottom = box.bottom() - static_cast<double>(cap) / 2.0;
    for (double y = std::ceil(y_top); y <= std::floor(y_bottom); y += 1.0) {
        emit(out, x0, y, kVertical);
        emit(out, x1, y, kVertical);
    }
}

std::vector<CellChar> build_shape(ShapeKind kind, Rect box) {
    std::vector<CellChar> cells;
    switch (kind) {
        case ShapeKind::Box:
            emit_box(cells, box, false);
            break;
        case ShapeKind::Rounded:
            emit_box(cells, box, true);
            break;
        case ShapeKind::Diamond:
            emit_row_based(cells, box, false);
            break;
        case ShapeKind::Ellipse:
            emit_row_based(cells, box, true);
            break;
        case ShapeKind::Stadium:
            emit_stadium(cells, box);
            break;
        case ShapeKind::Hexagon:
            emit_hexagon(cells, box);
            break;
        case ShapeKind::Cylinder:
            emit_cylinder(cells, box);
            break;
        case ShapeKind::Icon:
            break;  // 无边框
    }
    return cells;
}

}  // namespace

std::vector<Point> shape_outline(ShapeKind kind, Rect box) {
    const std::vector<CellChar> cells = build_shape(kind, box);
    std::vector<Point> points;
    points.reserve(cells.size());
    for (const CellChar& cell : cells) {
        points.push_back(cell.first);
    }
    return points;
}

void draw_shape(canvas::Canvas& canvas, ShapeKind kind, Rect box, const Style& style) {
    for (const CellChar& cell : build_shape(kind, box)) {
        canvas.put(cell.first, cell.second, style);
    }
}

}  // namespace termchart::graph
