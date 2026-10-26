// hexbin.cpp —— 六边形分箱实现：偏移网格桶计数 + 分级着色
#include "termchart/chart/geo/hexbin.hpp"
#include "termchart/chart/scene_sink.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

constexpr char32_t kHex = U'\u2B22';  // ⬢

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

void Hexbin::draw(DrawContext ctx) {
    // 桶参数收敛：半径 >= 1 且有限。
    double r = std::isfinite(options_.radius) ? options_.radius : 1.0;
    r = std::max(1.0, r);
    const int bins = std::min(8, std::max(1, options_.bins));

    // 分桶：奇数行右移 r（偏移网格），桶键 (row, col)。
    std::map<std::pair<long long, long long>, int> counts;
    for (const Point& p : points_) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
            continue;
        }
        const long long row = std::llround(p.y / r);
        const double offset = (row % 2 != 0) ? r : 0.0;
        const long long col = std::llround((p.x - offset) / (2.0 * r));
        ++counts[std::make_pair(row, col)];
    }
    if (counts.empty()) {
        return;
    }

    int cmin = 0;
    int cmax = 0;
    bool have = false;
    for (const auto& kv : counts) {
        if (!have) {
            cmin = cmax = kv.second;
            have = true;
        } else {
            cmin = std::min(cmin, kv.second);
            cmax = std::max(cmax, kv.second);
        }
    }

    const Rect area = content_area(ctx, *this);
    for (const auto& kv : counts) {
        double frac = cmax > cmin
                          ? static_cast<double>(kv.second - cmin) /
                                static_cast<double>(cmax - cmin)
                          : 0.0;
        if (bins > 1) {
            const auto idx = static_cast<int>(std::llround(frac * (bins - 1)));
            frac = static_cast<double>(idx) / static_cast<double>(bins - 1);
        } else {
            frac = 0.0;
        }
        const Style st = Style{}.set_fg(ramp_color(frac));

        // 桶中心 → 内容区格坐标；越界桶丢弃。
        const double offset = (kv.first.first % 2 != 0) ? r : 0.0;
        const double px =
            area.x + static_cast<double>(kv.first.second) * 2.0 * r + offset;
        const double py = area.y + static_cast<double>(kv.first.first) * r;
        const auto cx = std::llround(px);
        const auto cy = std::llround(py);
        if (cx < 0 || cy < 0 || cx >= static_cast<long long>(ctx.canvas.width()) ||
            cy >= static_cast<long long>(ctx.canvas.height())) {
            continue;
        }
        ctx.canvas.put(static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cy),
                       kHex, st);
    }
}

bool Hexbin::to_scene(scene::Scene& out, std::uint32_t width,
                      std::uint32_t height) const {
    // 桶参数收敛：半径 >= 1 且有限。
    double r = std::isfinite(options_.radius) ? options_.radius : 1.0;
    r = std::max(1.0, r);
    const int bins = std::min(8, std::max(1, options_.bins));

    // 分桶：奇数行右移 r（偏移网格），桶键 (row, col)。
    std::map<std::pair<long long, long long>, int> counts;
    for (const Point& p : points_) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
            continue;
        }
        const long long row = std::llround(p.y / r);
        const double offset = (row % 2 != 0) ? r : 0.0;
        const long long col = std::llround((p.x - offset) / (2.0 * r));
        ++counts[std::make_pair(row, col)];
    }
    if (counts.empty()) {
        return true;
    }

    int cmin = 0;
    int cmax = 0;
    bool have = false;
    for (const auto& kv : counts) {
        if (!have) {
            cmin = cmax = kv.second;
            have = true;
        } else {
            cmin = std::min(cmin, kv.second);
            cmax = std::max(cmax, kv.second);
        }
    }

    SceneSink sink(out);
    const Rect area = content_area(sink, width, height, *this);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    for (const auto& kv : counts) {
        double frac = cmax > cmin
                          ? static_cast<double>(kv.second - cmin) /
                                static_cast<double>(cmax - cmin)
                          : 0.0;
        if (bins > 1) {
            const auto idx = static_cast<int>(std::llround(frac * (bins - 1)));
            frac = static_cast<double>(idx) / static_cast<double>(bins - 1);
        } else {
            frac = 0.0;
        }
        const Style st = Style{}.set_fg(ramp_color(frac));

        // 桶中心 → 内容区格坐标；越界桶丢弃。
        const double offset = (kv.first.first % 2 != 0) ? r : 0.0;
        const double px =
            area.x + static_cast<double>(kv.first.second) * 2.0 * r + offset;
        const double py = area.y + static_cast<double>(kv.first.first) * r;
        const auto cx = std::llround(px);
        const auto cy = std::llround(py);
        if (cx < 0 || cy < 0 || cx >= static_cast<long long>(width) ||
            cy >= static_cast<long long>(height)) {
            continue;
        }
        sink.put(Point{static_cast<double>(cx), static_cast<double>(cy)}, kHex, st);
    }

    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
