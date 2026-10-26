// tile_grid.cpp —— 瓦片网格实现：一区域一格 + 值分级
#include "termchart/chart/geo/tile_grid.hpp"
#include "termchart/chart/scene_sink.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/utf8.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::chart {
namespace {

constexpr char32_t kBlock = U'\u2588';  // █（空名时的瓦片字符）

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

Rect content_area(SceneSink& sink, std::uint32_t w, std::uint32_t h, const Chart& chart) {
    const double W = static_cast<double>(w);
    const double H = static_cast<double>(h);
    Rect area{chart.margin_left(), chart.margin_top(),
              std::max(0.0, W - chart.margin_left() - chart.margin_right()),
              std::max(0.0, H - chart.margin_top() - chart.margin_bottom())};
    if (!chart.title().empty() && area.h >= 2.0) {
        layout::draw_title(sink, chart.title(), area);
        area = Rect{area.x, area.y + 1.0, area.w, area.h - 1.0};
    }
    return area;
}

Color ramp_color(double fraction01) noexcept {
    double f = std::isfinite(fraction01) ? fraction01 : 0.0;
    f = std::min(1.0, std::max(0.0, f));
    const auto level = static_cast<std::uint8_t>(23 - std::llround(f * 23.0));
    return Color::indexed(static_cast<std::uint8_t>(232 + level));
}

}  // namespace

void TileGrid::draw(DrawContext ctx) {
    if (tiles_.empty()) {
        return;
    }

    double vmin = 0.0;
    double vmax = 0.0;
    bool have = false;
    for (const GeoTile& t : tiles_) {
        const double v = std::isfinite(t.value) ? t.value : 0.0;
        if (!have) {
            vmin = vmax = v;
            have = true;
        } else {
            vmin = std::min(vmin, v);
            vmax = std::max(vmax, v);
        }
    }

    const Rect area = content_area(ctx, *this);
    const int bins = std::min(8, std::max(1, options_.bins));

    for (const GeoTile& t : tiles_) {
        const double v = std::isfinite(t.value) ? t.value : 0.0;
        double frac = vmax > vmin ? (v - vmin) / (vmax - vmin) : 0.0;
        frac = std::min(1.0, std::max(0.0, frac));
        if (bins > 1) {
            const auto idx = static_cast<int>(std::llround(frac * (bins - 1)));
            frac = static_cast<double>(idx) / static_cast<double>(bins - 1);
        } else {
            frac = 0.0;
        }
        const Style st = Style{}.set_fg(ramp_color(frac));

        // 瓦片位置 = 内容区原点 + (col, row)；越界瓦片整块丢弃。
        const double tx = area.x + static_cast<double>(t.col);
        const double ty = area.y + static_cast<double>(t.row);
        if (!(tx >= area.x && tx < area.right() && ty >= area.y && ty < area.bottom())) {
            continue;
        }

        if (t.name.empty()) {
            ctx.canvas.put(Point{tx, ty}, kBlock, st);
        } else {
            // 首簇作瓦片字符：按显示宽度取 1 列的前缀（宽字符自然占 2 格）。
            const auto keep = util::prefix_by_width(t.name, 1);
            if (keep == 0) {
                ctx.canvas.put(Point{tx, ty}, kBlock, st);
            } else {
                ctx.canvas.draw_text(Point{tx, ty},
                                     std::string_view{t.name}.substr(0, keep), st);
            }
        }
    }
}

bool TileGrid::to_scene(scene::Scene& out, std::uint32_t width,
                        std::uint32_t height) const {
    if (tiles_.empty()) {
        return true;
    }

    double vmin = 0.0;
    double vmax = 0.0;
    bool have = false;
    for (const GeoTile& t : tiles_) {
        const double v = std::isfinite(t.value) ? t.value : 0.0;
        if (!have) {
            vmin = vmax = v;
            have = true;
        } else {
            vmin = std::min(vmin, v);
            vmax = std::max(vmax, v);
        }
    }

    SceneSink sink(out);
    const Rect area = content_area(sink, width, height, *this);
    const int bins = std::min(8, std::max(1, options_.bins));

    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    for (const GeoTile& t : tiles_) {
        const double v = std::isfinite(t.value) ? t.value : 0.0;
        double frac = vmax > vmin ? (v - vmin) / (vmax - vmin) : 0.0;
        frac = std::min(1.0, std::max(0.0, frac));
        if (bins > 1) {
            const auto idx = static_cast<int>(std::llround(frac * (bins - 1)));
            frac = static_cast<double>(idx) / static_cast<double>(bins - 1);
        } else {
            frac = 0.0;
        }
        const Style st = Style{}.set_fg(ramp_color(frac));

        // 瓦片位置 = 内容区原点 + (col, row)；越界瓦片整块丢弃。
        const double tx = area.x + static_cast<double>(t.col);
        const double ty = area.y + static_cast<double>(t.row);
        if (!(tx >= area.x && tx < area.right() && ty >= area.y && ty < area.bottom())) {
            continue;
        }

        if (t.name.empty()) {
            sink.put(Point{tx, ty}, kBlock, st);
        } else {
            // 首簇作瓦片字符：按显示宽度取 1 列的前缀（宽字符自然占 2 格）。
            const auto keep = util::prefix_by_width(t.name, 1);
            if (keep == 0) {
                sink.put(Point{tx, ty}, kBlock, st);
            } else {
                sink.text(Point{tx, ty}, std::string_view{t.name}.substr(0, keep), st);
            }
        }
    }

    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
