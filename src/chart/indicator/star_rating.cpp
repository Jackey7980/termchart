// star_rating.cpp —— 星级评分实现：★/◐/☆ 一行排开
#include "termchart/chart/indicator/star_rating.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/core/attr.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

constexpr char32_t kFull = U'\u2605';   // ★
constexpr char32_t kHalf = U'\u25D0';   // ◐
constexpr char32_t kEmpty = U'\u2606';  // ☆

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

}  // namespace

void StarRating::draw(DrawContext ctx) {
    const Rect area = content_area(ctx, *this);
    if (area.w < 1.0 || area.h < 1.0) {
        return;
    }

    const auto max_stars = static_cast<std::size_t>(
        std::min(20, std::max(1, options_.max_stars)));

    // 评分收敛到 [0, max]，非有限按 0；四舍五入到半星。
    double rating = std::isfinite(rating_) ? rating_ : 0.0;
    rating = std::min(static_cast<double>(max_stars), std::max(0.0, rating));
    const double halves = static_cast<double>(std::llround(rating * 2.0));
    const auto full = static_cast<std::size_t>(halves / 2.0);
    const bool half = (static_cast<long long>(halves) % 2) != 0;

    const Style filled = Style{}.set_fg(options_.star_color).set_attr(Attr::Bold);
    const Style empty_st = options_.dim_empty
                               ? Style{}.set_fg(default_color(DefaultColor::Muted))
                               : Style{}.set_fg(options_.star_color);

    const double row = area.y + (area.h - 1.0) / 2.0;
    for (std::size_t i = 0; i < max_stars; ++i) {
        const double x = area.x + static_cast<double>(i);
        if (x >= area.right()) {
            break;  // 画布装不下全部星：截断，不折行
        }
        if (i < full) {
            ctx.canvas.put(Point{x, row}, kFull, filled);
        } else if (i == full && half) {
            ctx.canvas.put(Point{x, row}, kHalf, filled);
        } else {
            ctx.canvas.put(Point{x, row}, kEmpty, empty_st);
        }
    }
}

}  // namespace termchart::chart
