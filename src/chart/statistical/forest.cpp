// forest.cpp —— 森林图实现：逐行 CI 横线 + 点估计标记
#include "termchart/chart/statistical/forest.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// 按码点截断（UTF-8），残缺序列丢弃。
std::string_view clip_utf8(std::string_view s, std::size_t max_chars) noexcept {
    std::size_t chars = 0;
    std::size_t i = 0;
    while (i < s.size() && chars < max_chars) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t len = 1;
        if (c >= 0x80) {
            len = (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
        }
        if (i + len > s.size()) {
            break;
        }
        i += len;
        ++chars;
    }
    return s.substr(0, i);
}

}  // namespace

void ForestChart::set_studies(std::vector<ForestStudy> studies) {
    studies_ = std::move(studies);
}

void ForestChart::draw(DrawContext ctx) {
    // 行序：研究自上而下，合计行收尾。行 i 落在数据 y = i（y 轴反转）。
    std::size_t rows = studies_.size() + (has_total_ ? 1 : 0);
    if (rows == 0) {
        return;
    }
    // 只收有限且 lower ≤ estimate ≤ upper 的行（乱序 CI 不画）。
    double lo = 0.0;  // x 域并入 0
    double hi = 0.0;
    bool any = false;
    auto widen = [&](const ForestStudy& s) {
        if (!std::isfinite(s.estimate) || !std::isfinite(s.lower) ||
            !std::isfinite(s.upper) || s.lower > s.upper) {
            return false;
        }
        lo = std::min(lo, s.lower);
        hi = std::max(hi, s.upper);
        any = true;
        return true;
    };
    for (const ForestStudy& s : studies_) {
        widen(s);
    }
    if (has_total_) {
        widen(total_);
    }
    if (!any) {
        return;
    }
    if (!(hi > lo)) {
        lo -= 0.5;
        hi += 0.5;
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, lo, hi, -0.5, static_cast<double>(rows) - 0.5,
                      false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, {}, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "effect", ctx.axis_style);
    }
    if (options_.show_zero_line && lo < 0.0 && hi > 0.0) {
        // 无效应参考线：x = 0 的竖直点线。
        const double x0 = area_.x_scale().forward(0.0);
        const Rect inner = area_.inner_rect();
        ctx.canvas.draw_line(Point{x0, inner.y}, Point{x0, inner.bottom()}, U'\u00B7',
                             Style{}.set_fg(default_color(DefaultColor::Muted)));
    }

    const Style st = Style{}.set_fg(options_.color);
    std::size_t row = 0;
    for (const ForestStudy& s : studies_) {
        if (!std::isfinite(s.estimate) || !std::isfinite(s.lower) ||
            !std::isfinite(s.upper) || s.lower > s.upper) {
            continue;  // 坏行不占行号：有效行连续排布
        }
        const double y = area_.y_scale().forward(static_cast<double>(row));
        ctx.canvas.draw_line(Point{area_.x_scale().forward(s.lower), y},
                             Point{area_.x_scale().forward(s.upper), y},
                             U'\u2500', st);
        ctx.canvas.put(Point{area_.x_scale().forward(s.estimate), y},
                       kMarkerChar, st);
        ++row;
    }
    if (has_total_ && std::isfinite(total_.estimate) && std::isfinite(total_.lower) &&
        std::isfinite(total_.upper) && total_.lower <= total_.upper) {
        const double y = area_.y_scale().forward(static_cast<double>(row));
        ctx.canvas.draw_line(Point{area_.x_scale().forward(total_.lower), y},
                             Point{area_.x_scale().forward(total_.upper), y},
                             U'\u2500', st);
        ctx.canvas.put(Point{area_.x_scale().forward(total_.estimate), y},
                       U'\u25C6', st);  // ◆ 合计
    }

    // 行标签写在左侧留白区。
    const std::size_t max_chars =
        static_cast<std::size_t>(m.left >= 1.0 ? static_cast<std::size_t>(m.left) - 1 : 0);
    if (max_chars > 0) {
        std::size_t label_row = 0;
        for (const ForestStudy& s : studies_) {
            if (!std::isfinite(s.estimate) || !std::isfinite(s.lower) ||
                !std::isfinite(s.upper) || s.lower > s.upper) {
                continue;
            }
            if (!s.label.empty()) {
                const double y = area_.y_scale().forward(static_cast<double>(label_row));
                ctx.canvas.draw_text(Point{0.0, y}, clip_utf8(s.label, max_chars),
                                     Style{}.set_fg(default_color(DefaultColor::Series)));
            }
            ++label_row;
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area_.inner_rect());
    }
}

}  // namespace termchart::chart
