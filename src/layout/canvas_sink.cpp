// canvas_sink.cpp —— Sink → Canvas 的转发实现
#include "termchart/layout/canvas_sink.hpp"

#include <cmath>

namespace termchart::layout {

CanvasSink::CanvasSink(canvas::Canvas& canvas) noexcept : canvas_(&canvas) {}

void CanvasSink::put(Point at, char32_t cp, const Style& style) {
    canvas_->put(at, cp, style);
}

void CanvasSink::text(Point at, std::string_view value, const Style& style) {
    canvas_->draw_text(at, value, style);
}

void CanvasSink::line(Point a, Point b, LineStyle kind, const Style& style) {
    // layout 只产生水平/竖直线段。斜线不在 Sink 的承诺里（那是数据几何，
    // 归 chart 自己的 Scene 路径），这里按起点所在方向兜底。
    const bool horizontal = std::fabs(a.y - b.y) < std::fabs(a.x - b.x);
    char32_t cp = kSolidH;
    if (kind == LineStyle::Solid) {
        cp = horizontal ? kSolidH : kSolidV;
    } else {
        cp = horizontal ? kDashedH : kDashedV;
    }
    canvas_->draw_line(a, b, cp, style);
}

}  // namespace termchart::layout
