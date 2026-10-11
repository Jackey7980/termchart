// fill.hpp —— 多边形扫描线填充（含纵向超采样）
//
// 扫描线思路：按**格行**推进，对每条采样线求它与多边形各边的交点，排序后按
// 奇偶规则配对成若干半开区间 [x0, x1)，区间内的格才点亮。
//
// 为什么还要超采样：字符格没有灰度，边界格"点亮与否"直接决定图形边缘的观感。
// supersample = n 时每格纵向取 n 条采样线，横向用**精确重叠长度**算覆盖比例，
// 覆盖率 >= coverage 才写入 —— 边界格因此会按实际覆盖面积决定取舍，
// 比"格中心在内就点亮"稳得多（细长三角形不会被画丢）。
#ifndef TERMCHART_CANVAS_FILL_HPP
#define TERMCHART_CANVAS_FILL_HPP

#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {

// 一个格行内的半开区间 [x0, x1)，row 为格行号（非负）。
struct Span {
    std::uint32_t row = 0;
    double x0 = 0.0;
    double x1 = 0.0;
};

struct FillOptions {
    char32_t on = U'\u2588';       // 填充字符（实心块）
    Style style{};
    std::uint32_t supersample = 1;  // 每格纵向子采样数（1 = 只取格中心）
    double coverage = 0.5;          // 写入阈值：格被覆盖的比例
};

// 单条采样线上的区间（y 为像素坐标）；便于单测与复用。
[[nodiscard]] TERMCHART_API std::vector<Span> scanline_spans(
    const std::vector<Point>& polygon);

// 逐格填充多边形（奇偶规则），返回实际写入画布（未越界）的格数。
TERMCHART_API std::size_t fill_polygon(Canvas& canvas, const std::vector<Point>& polygon,
                                       const FillOptions& opts = {});

}  // namespace termchart::canvas

#endif  // TERMCHART_CANVAS_FILL_HPP
