// progress.cpp —— 进度条组实现：标签 + █/░ 液条 + 百分比
#include "termchart/chart/text/progress.hpp"

#include "termchart/chart/scene_sink.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::chart {
namespace {

constexpr char32_t kFill = U'\u2588';   // █
constexpr char32_t kEmpty = U'\u2591';  // ░

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

// 非有限/越界的 fraction 收敛到 [0, 1]。
double clamp_fraction(double f) noexcept {
    if (!std::isfinite(f)) {
        return 0.0;
    }
    return std::min(1.0, std::max(0.0, f));
}

}  // namespace

void Progress::add_bar(std::string label, double fraction, Color color) {
    bars_.push_back(Bar{std::move(label), clamp_fraction(fraction), color});
}

void Progress::draw(DrawContext ctx) {
    if (bars_.empty()) {
        return;
    }

    const Rect area = content_area(ctx, *this);
    if (area.w < 1.0 || area.h < 1.0) {
        return;
    }

    // 标签列宽 = 最大显示宽度，封顶内容区一半。
    std::size_t label_w = 0;
    for (const Bar& b : bars_) {
        label_w = std::max(label_w, static_cast<std::size_t>(
                                        std::max(0, util::display_width(b.label))));
    }
    double label_d = static_cast<double>(label_w);
    label_d = std::min(label_d, area.w / 2.0);

    const double percent_d = options_.show_percent ? 5.0 : 0.0;  // "100% " 
    double bar_d = options_.bar_width != 0
                       ? static_cast<double>(options_.bar_width)
                       : area.w - label_d - 1.0 - percent_d;
    if (bar_d < 1.0) {
        bar_d = 1.0;  // 画布再小也至少画一格液条
    }
    const auto bar_w = static_cast<std::size_t>(bar_d);

    double row = area.y;
    for (const Bar& b : bars_) {
        if (row >= area.bottom()) {
            return;
        }
        if (label_d >= 1.0) {
            const auto keep =
                util::prefix_by_width(b.label, static_cast<int>(label_d));
            ctx.canvas.draw_text(Point{area.x, row},
                                 std::string_view{b.label}.substr(0, keep), Style{});
        }

        double x = area.x + label_d + (label_d > 0.0 ? 1.0 : 0.0);
        const auto filled =
            static_cast<std::size_t>(std::llround(b.fraction * static_cast<double>(bar_w)));
        const Style st = Style{}.set_fg(b.color);
        for (std::size_t k = 0; k < bar_w && x < area.right(); ++k, x += 1.0) {
            ctx.canvas.put(Point{x, row}, k < filled ? kFill : kEmpty, st);
        }

        if (options_.show_percent && x + 3.0 <= area.right()) {
            char buf[16];
            const int pct = static_cast<int>(b.fraction * 100.0 + 0.5);
            std::snprintf(buf, sizeof(buf), "%d%%", pct);
            ctx.canvas.draw_text(Point{x + 1.0, row}, buf, Style{});
        }
        row += 1.0;
    }
}

bool Progress::to_scene(scene::Scene& out, std::uint32_t width,
                        std::uint32_t height) const {
    if (bars_.empty()) {
        return true;
    }

    SceneSink sink(out);
    const double w = static_cast<double>(width);
    const double h = static_cast<double>(height);
    Rect area{margin_left(), margin_top(),
              std::max(0.0, w - margin_left() - margin_right()),
              std::max(0.0, h - margin_top() - margin_bottom())};
    if (!title_.empty() && area.h >= 2.0) {
        layout::draw_title(sink, title_, area);
        area = Rect{area.x, area.y + 1.0, area.w, area.h - 1.0};
    }
    if (area.w < 1.0 || area.h < 1.0) {
        return true;
    }

    // 标签列宽 = 最大显示宽度，封顶内容区一半。
    std::size_t label_w = 0;
    for (const Bar& b : bars_) {
        label_w = std::max(label_w, static_cast<std::size_t>(
                                        std::max(0, util::display_width(b.label))));
    }
    double label_d = static_cast<double>(label_w);
    label_d = std::min(label_d, area.w / 2.0);

    const double percent_d = options_.show_percent ? 5.0 : 0.0;  // "100% "
    double bar_d = options_.bar_width != 0
                       ? static_cast<double>(options_.bar_width)
                       : area.w - label_d - 1.0 - percent_d;
    if (bar_d < 1.0) {
        bar_d = 1.0;  // 画布再小也至少画一格液条
    }
    const auto bar_w = static_cast<std::size_t>(bar_d);

    scene::SceneBuilder& sb = sink.builder();
    sb.begin_group(0);

    double row = area.y;
    for (const Bar& b : bars_) {
        if (row >= area.bottom()) {
            break;
        }
        if (label_d >= 1.0) {
            const auto keep =
                util::prefix_by_width(b.label, static_cast<int>(label_d));
            sink.text(Point{area.x, row}, std::string_view{b.label}.substr(0, keep),
                      Style{});
        }

        double x = area.x + label_d + (label_d > 0.0 ? 1.0 : 0.0);
        const auto filled = static_cast<std::size_t>(
            std::llround(b.fraction * static_cast<double>(bar_w)));
        const Style st = Style{}.set_fg(b.color);
        for (std::size_t k = 0; k < bar_w && x < area.right(); ++k, x += 1.0) {
            sink.put(Point{x, row}, k < filled ? kFill : kEmpty, st);
        }

        if (options_.show_percent && x + 3.0 <= area.right()) {
            char buf[16];
            const int pct = static_cast<int>(b.fraction * 100.0 + 0.5);
            std::snprintf(buf, sizeof(buf), "%d%%", pct);
            sink.text(Point{x + 1.0, row}, buf, Style{});
        }
        row += 1.0;
    }

    sb.end_group(0);
    return true;
}

}  // namespace termchart::chart
