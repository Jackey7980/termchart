// canvas_sink.hpp —— 把布局落笔到字符格
//
// 行为与 Sink 化之前**逐字节一致**：同一对端点、同一个字符、同一个样式，
// 调的都是同一个 Canvas 方法。它存在只是为了把 layout 的布局逻辑从
// "必须持有 Canvas"里解耦出来，好让 SceneSink 复用同一份布局。
#ifndef TERMCHART_LAYOUT_CANVAS_SINK_HPP
#define TERMCHART_LAYOUT_CANVAS_SINK_HPP

#include <string_view>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/sink.hpp"

namespace termchart::layout {

class CanvasSink final : public Sink {
public:
    explicit CanvasSink(canvas::Canvas& canvas) noexcept;

    void put(Point at, char32_t cp, const Style& style) override;
    void text(Point at, std::string_view value, const Style& style) override;
    void line(Point a, Point b, LineStyle kind, const Style& style) override;

private:
    canvas::Canvas* canvas_;
};

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_CANVAS_SINK_HPP
