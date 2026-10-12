// status_light.cpp —— 状态灯实现：● 着色 + 可选标签
#include "termchart/chart/indicator/status_light.hpp"

#include <algorithm>
#include <utility>

#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::chart {
namespace {

constexpr char32_t kLight = U'\u25CF';  // ●

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

Color state_color(LightState state) noexcept {
    switch (state) {
        case LightState::Ok:
            return Color::ansi16(2);     // 绿
        case LightState::Warning:
            return Color::ansi16(3);     // 黄
        case LightState::Error:
            return Color::ansi16(1);     // 红
        case LightState::Unknown:
            break;
    }
    return Color::ansi16(8);             // 灰
}

}  // namespace

void StatusLight::draw(DrawContext ctx) {
    const Rect area = content_area(ctx, *this);
    if (area.w < 1.0 || area.h < 1.0) {
        return;
    }

    // 内容区垂直居中一格。
    const double row = area.y + (area.h - 1.0) / 2.0;
    const Style st = Style{}.set_fg(state_color(state_));
    ctx.canvas.put(Point{area.x, row}, kLight, st);

    if (options_.show_label && !label_.empty() && area.w >= 3.0) {
        const auto keep =
            util::prefix_by_width(label_, static_cast<int>(area.w - 2.0));
        if (keep > 0) {
            ctx.canvas.draw_text(Point{area.x + 2.0, row},
                                 std::string_view{label_}.substr(0, keep), st);
        }
    }
}

}  // namespace termchart::chart
