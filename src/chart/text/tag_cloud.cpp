// tag_cloud.cpp —— 标签云实现：降序 + 流式换行排版
#include "termchart/chart/text/tag_cloud.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "termchart/core/attr.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::chart {
namespace {

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

Color color_for_rank(std::size_t rank) noexcept {
    static const Color kPalette[] = {
        default_color(DefaultColor::Cyan), default_color(DefaultColor::Accent), default_color(DefaultColor::Amber),
        Color::ansi16(5), default_color(DefaultColor::Neutral), default_color(DefaultColor::Series),
    };
    return kPalette[rank % (sizeof(kPalette) / sizeof(kPalette[0]))];
}

}  // namespace

void TagCloud::draw(DrawContext ctx) {
    placed_ = 0;

    std::vector<Tag> tags;
    tags.reserve(tags_.size());
    for (const Tag& t : tags_) {
        if (!t.text.empty() && t.weight > 0.0 && std::isfinite(t.weight)) {
            tags.push_back(t);
        }
    }
    std::sort(tags.begin(), tags.end(),
              [](const Tag& a, const Tag& b) { return a.weight > b.weight; });
    if (tags.empty()) {
        return;
    }

    const Rect area = content_area(ctx, *this);
    if (area.w < 1.0 || area.h < 1.0) {
        return;
    }

    double row = area.y;
    double col = area.x;
    for (std::size_t rank = 0; rank < tags.size(); ++rank) {
        const Tag& t = tags[rank];
        const int tag_w = util::display_width(t.text);
        if (tag_w <= 0) {
            continue;
        }
        if (static_cast<double>(tag_w) > area.w) {
            continue;  // 单标签独占不了整行：放弃，不截断
        }
        // 放不下 → 换行；行数用尽 → 收工。
        if (col + static_cast<double>(tag_w) - 1.0 > area.right()) {
            col = area.x;
            row += 1.0;
            if (row >= area.bottom()) {
                return;
            }
        }

        Style style = Style{}.set_fg(color_for_rank(rank));
        if (rank < options_.bold_top) {
            style.set_attr(Attr::Bold);
        }
        ctx.canvas.draw_text(Point{col, row}, t.text, style);
        ++placed_;
        col += static_cast<double>(tag_w) + static_cast<double>(options_.gap);
    }
}

}  // namespace termchart::chart
