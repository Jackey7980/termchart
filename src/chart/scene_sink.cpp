// scene_sink.cpp —— Sink → Scene 的翻译
#include "termchart/chart/scene_sink.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

#include "termchart/layout/sink.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::chart {
namespace {

std::string repeat_utf8(char32_t cp, std::size_t count) {
    const std::string unit = util::to_utf8(cp);
    std::string out;
    out.reserve(unit.size() * count);
    for (std::size_t i = 0; i < count; ++i) {
        out += unit;
    }
    return out;
}

}  // namespace

SceneSink::SceneSink(scene::Scene& scene) noexcept : builder_(scene) {}

void SceneSink::put(Point at, char32_t cp, const Style& style) {
    builder_.text(at, util::to_utf8(cp), style);
}

void SceneSink::text(Point at, std::string_view value, const Style& style) {
    builder_.text(at, value, style);
}

void SceneSink::line(Point a, Point b, layout::LineStyle kind, const Style& style) {
    const double ax = std::floor(a.x);
    const double ay = std::floor(a.y);
    const double bx = std::floor(b.x);
    const double by = std::floor(b.y);
    const bool axis_h = ay == by;
    const bool axis_v = ax == bx;

    // 轴/边框/网格一律用终端原生字符（─│┄┆），与 CanvasSink 逐格一致。
    // 为什么不让实线也走矢量：盲文光栅会把 `│` 画成 `⣿`、把 `─` 画成 `⠛`，
    // 一条细轴线立刻胖成两像素宽的实心带——比字符更糊，不是更准。
    // 矢量只用在**非轴向**的几何上（数据折线的斜段、将来的曲线），那里盲文
    // 的 2×4 子像素确实优于字符台阶。
    if (axis_h) {
        const double n = std::max(0.0, std::max(ax, bx) - std::min(ax, bx)) + 1.0;
        builder_.text(Point{std::min(ax, bx), ay},
                      repeat_utf8(kind == layout::LineStyle::Solid ? layout::kSolidH
                                                                   : layout::kDashedH,
                                  static_cast<std::size_t>(n)),
                      style);
        return;
    }
    if (axis_v) {
        const char32_t cp =
            kind == layout::LineStyle::Solid ? layout::kSolidV : layout::kDashedV;
        const std::string unit = util::to_utf8(cp);
        const double y0 = std::min(ay, by);
        const double y1 = std::max(ay, by);
        for (double y = y0; y <= y1; y += 1.0) {
            builder_.text(Point{ax, y}, unit, style);
        }
        return;
    }

    // 斜向：矢量描边。StrokeOp 只有颜色与宽度，下划线属性不参与描边。
    builder_.stroke(style.fg(), 1.0).move_to(a).line_to(b);
}

}  // namespace termchart::chart
