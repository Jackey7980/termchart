// gauge_kpi.cpp —— KPI 卡片实现：边框 + 标签/主值/变化量
#include "termchart/chart/indicator/gauge_kpi.hpp"

#include <algorithm>
#include <utility>

#include "termchart/core/attr.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/chart/scene_sink.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::chart {
namespace {

constexpr char32_t kCornerTl = U'\u250C';  // ┌
constexpr char32_t kCornerTr = U'\u2510';  // ┐
constexpr char32_t kCornerBl = U'\u2514';  // └
constexpr char32_t kCornerBr = U'\u2518';  // ┘
constexpr char32_t kHLine = U'\u2500';     // ─
constexpr char32_t kVLine = U'\u2502';     // │
constexpr char32_t kUp = U'\u25B2';        // ▲
constexpr char32_t kDown = U'\u25BC';      // ▼

Rect content_area(const DrawContext& ctx, const Chart& chart) {
    const double w = static_cast<double>(ctx.canvas.width());
    const double h = static_cast<double>(ctx.canvas.height());
    Rect area{chart.margin_left(), chart.margin_top(),
              std::max(0.0, w - chart.margin_left() - chart.margin_right()),
              std::max(0.0, h - chart.margin_top() - chart.margin_bottom())};
    if (ctx.draw_title && !chart.title().empty() && area.h >= 2.0) {
        layout::draw_title(ctx.canvas, chart.title(), area);
        area = Rect{area.x, area.y + 1.0, area.w, area.h - 1.0};
    }
    return area;
}

// 居中放一段文本（按显示宽度），起点越界时贴 0。
void draw_centered(canvas::Canvas& canvas, double cx, double y,
                   std::string_view text, const Style& st) noexcept {
    const int w = util::display_width(text);
    const double x = cx - static_cast<double>(w) / 2.0;
    canvas.draw_text(Point{x < 0.0 ? 0.0 : x, y}, text, st);
}

std::string_view clip(std::string_view text, double budget) noexcept {
    const auto b = static_cast<int>(budget);
    if (b <= 0) {
        return {};
    }
    return text.substr(0, util::prefix_by_width(text, b));
}

// Sink 版：标题走 layout::draw_title，内容区据此缩进一行（与 DrawContext 版同源）。
Rect content_area(SceneSink& sink, std::uint32_t canvas_w,
                  std::uint32_t canvas_h, const Chart& chart) {
    const double w = static_cast<double>(canvas_w);
    const double h = static_cast<double>(canvas_h);
    Rect area{chart.margin_left(), chart.margin_top(),
              std::max(0.0, w - chart.margin_left() - chart.margin_right()),
              std::max(0.0, h - chart.margin_top() - chart.margin_bottom())};
    if (!chart.title().empty() && area.h >= 2.0) {
        layout::draw_title(sink, chart.title(), area);
        area = Rect{area.x, area.y + 1.0, area.w, area.h - 1.0};
    }
    return area;
}

// 居中放一段文本（按显示宽度），起点越界时贴 0。
void draw_centered(SceneSink& sink, double cx, double y,
                   std::string_view text, const Style& st) noexcept {
    const int w = util::display_width(text);
    const double x = cx - static_cast<double>(w) / 2.0;
    sink.text(Point{x < 0.0 ? 0.0 : x, y}, text, st);
}

}  // namespace

void GaugeKpi::draw(DrawContext ctx) {
    const Rect area = content_area(ctx, *this);
    // 太小画不了卡片：至少把主值贴左上画出来（有就画，没有就空）。
    if (area.w < 4.0 || area.h < 3.0) {
        if (!value_.empty() && area.w >= 1.0 && area.h >= 1.0) {
            ctx.canvas.draw_text(Point{area.x, area.y}, clip(value_, area.w), Style{});
        }
        return;
    }

    // 边框。
    const Style border{};
    ctx.canvas.put(Point{area.x, area.y}, kCornerTl, border);
    ctx.canvas.put(Point{area.right() - 1.0, area.y}, kCornerTr, border);
    ctx.canvas.put(Point{area.x, area.bottom() - 1.0}, kCornerBl, border);
    ctx.canvas.put(Point{area.right() - 1.0, area.bottom() - 1.0}, kCornerBr, border);
    for (double x = area.x + 1.0; x < area.right() - 1.0; x += 1.0) {
        ctx.canvas.put(Point{x, area.y}, kHLine, border);
        ctx.canvas.put(Point{x, area.bottom() - 1.0}, kHLine, border);
    }
    for (double y = area.y + 1.0; y < area.bottom() - 1.0; y += 1.0) {
        ctx.canvas.put(Point{area.x, y}, kVLine, border);
        ctx.canvas.put(Point{area.right() - 1.0, y}, kVLine, border);
    }

    const double cx = area.x + area.w / 2.0;
    const double inner_w = area.w - 2.0;

    // 标签（上）。
    double row = area.y + 1.0;
    if (!label_.empty()) {
        draw_centered(ctx.canvas, cx, row, clip(label_, inner_w),
                      Style{}.set_attr(Attr::Dim));
    }

    // 主值（中）：无标签时占首行，否则下移一行（空间够才下移）。
    if (!label_.empty() && area.h >= 4.0) {
        row += 1.0;
    }
    if (!value_.empty()) {
        draw_centered(ctx.canvas, cx, row, clip(value_, inner_w),
                      Style{}.set_attr(Attr::Bold));
    }

    // 变化量（下）：▲/▼ + 文本，整体居中；放不下就只画箭头。
    if (options_.show_delta && !delta_.empty() && area.bottom() - 2.0 > row) {
        const double dy = area.bottom() - 2.0;
        const Style dst =
            Style{}.set_fg(delta_up_ ? default_color(DefaultColor::Accent) : default_color(DefaultColor::Warn));
        const int text_w = util::display_width(delta_);
        const double total = 1.0 + 1.0 + static_cast<double>(text_w);
        double x = cx - total / 2.0;
        if (x < area.x + 1.0) {
            x = area.x + 1.0;
        }
        ctx.canvas.put(Point{x, dy}, delta_up_ ? kUp : kDown, dst);
        if (area.right() - 1.0 > x + 2.0) {
            ctx.canvas.draw_text(Point{x + 2.0, dy},
                                 clip(delta_, area.right() - 1.0 - (x + 2.0)), dst);
        }
    }
}

bool GaugeKpi::to_scene(scene::Scene& out, std::uint32_t width,
                        std::uint32_t height) const {
    SceneSink sink(out);
    const Rect area = content_area(sink, width, height, *this);

    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    // 太小画不了卡片：至少把主值贴左上画出来（有就画，没有就空）。
    if (area.w < 4.0 || area.h < 3.0) {
        if (!value_.empty() && area.w >= 1.0 && area.h >= 1.0) {
            sink.text(Point{area.x, area.y}, clip(value_, area.w), Style{});
        }
        b.end_group(0);
        return true;
    }

    // 边框。
    const Style border{};
    sink.put(Point{area.x, area.y}, kCornerTl, border);
    sink.put(Point{area.right() - 1.0, area.y}, kCornerTr, border);
    sink.put(Point{area.x, area.bottom() - 1.0}, kCornerBl, border);
    sink.put(Point{area.right() - 1.0, area.bottom() - 1.0}, kCornerBr, border);
    for (double x = area.x + 1.0; x < area.right() - 1.0; x += 1.0) {
        sink.put(Point{x, area.y}, kHLine, border);
        sink.put(Point{x, area.bottom() - 1.0}, kHLine, border);
    }
    for (double y = area.y + 1.0; y < area.bottom() - 1.0; y += 1.0) {
        sink.put(Point{area.x, y}, kVLine, border);
        sink.put(Point{area.right() - 1.0, y}, kVLine, border);
    }

    const double cx = area.x + area.w / 2.0;
    const double inner_w = area.w - 2.0;

    // 标签（上）。
    double row = area.y + 1.0;
    if (!label_.empty()) {
        draw_centered(sink, cx, row, clip(label_, inner_w), Style{}.set_attr(Attr::Dim));
    }

    // 主值（中）：无标签时占首行，否则下移一行（空间够才下移）。
    if (!label_.empty() && area.h >= 4.0) {
        row += 1.0;
    }
    if (!value_.empty()) {
        draw_centered(sink, cx, row, clip(value_, inner_w), Style{}.set_attr(Attr::Bold));
    }

    // 变化量（下）：▲/▼ + 文本，整体居中；放不下就只画箭头。
    if (options_.show_delta && !delta_.empty() && area.bottom() - 2.0 > row) {
        const double dy = area.bottom() - 2.0;
        const Style dst =
            Style{}.set_fg(delta_up_ ? default_color(DefaultColor::Accent) : default_color(DefaultColor::Warn));
        const int text_w = util::display_width(delta_);
        const double total = 1.0 + 1.0 + static_cast<double>(text_w);
        double x = cx - total / 2.0;
        if (x < area.x + 1.0) {
            x = area.x + 1.0;
        }
        sink.put(Point{x, dy}, delta_up_ ? kUp : kDown, dst);
        if (area.right() - 1.0 > x + 2.0) {
            sink.text(Point{x + 2.0, dy}, clip(delta_, area.right() - 1.0 - (x + 2.0)),
                      dst);
        }
    }

    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
