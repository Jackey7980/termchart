// bullet.cpp —— 子弹图实现（背景带 → 度量条 → 目标线，逐行绘制）
#include "termchart/chart/cartesian/bullet.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "termchart/canvas/canvas.hpp"
#include "termchart/detail/block_chars.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/chart/scene_sink.hpp"

namespace termchart::chart {
namespace {

// 三段背景带由淡到浓：数字越大代表"越好"的区间越亮，视觉上从差到优。
constexpr std::array<char32_t, 3> kBandChars{detail::kLightShade, detail::kMediumShade,
                                             detail::kDarkShade};

}  // namespace

void BulletChart::set_rows(std::vector<Row> rows) { rows_ = std::move(rows); }

void BulletChart::draw(DrawContext ctx) {
    std::vector<Row> rows;
    rows.reserve(rows_.size());
    for (const Row& r : rows_) {
        bool ok = std::isfinite(r.value) && std::isfinite(r.target);
        for (const double b : r.bands) {
            ok = ok && std::isfinite(b);
        }
        if (ok) {
            rows.push_back(r);
        }
    }
    if (rows.empty()) {
        return;
    }

    // x 定义域：所有带端点/度量/目标的最大值，底部从 0 起。
    double xmax = 0.0;
    for (const Row& r : rows) {
        xmax = std::max(xmax, r.value);
        xmax = std::max(xmax, r.target);
        for (const double b : r.bands) {
            xmax = std::max(xmax, b);
        }
    }
    if (!(xmax > 0.0)) {
        xmax = 1.0;  // 全零数据仍画出三段带（都为 0 宽），避免零宽定义域
    }
    const double pad = xmax * 0.05;

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, 0.0, xmax + pad, 0.0, static_cast<double>(rows.size()),
                      false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, {}, "", ctx.axis_style);
    }

    const Rect inner = area_.inner_rect();
    const Style target_st = Style{}.set_fg(default_color(DefaultColor::Warn));

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        const double y = area_.y_scale().forward(static_cast<double>(i) + 0.5);
        const Rect band_row{inner.x, y - 0.5, inner.w, 1.0};

        // 背景带：k 段从上一端点到本端点，统一裁进内框。
        double lo = 0.0;
        for (std::size_t k = 0; k < r.bands.size(); ++k) {
            const double hi = std::max(lo, r.bands[k]);
            const double x0 = area_.x_scale().forward(lo);
            const double x1 = area_.x_scale().forward(hi);
            const Rect seg = Rect{x0, band_row.y, x1 - x0, band_row.h}.intersection(inner);
            if (!seg.empty()) {
                ctx.canvas.fill_rect(seg, kBandChars[k], Style{});
            }
            lo = hi;
        }

        // 度量条：0 → value，实心块，盖在背景带之上。
        const double vx = area_.x_scale().forward(std::clamp(r.value, 0.0, xmax));
        const Rect bar = Rect{inner.x, y - 0.5, vx - inner.x, 1.0}.intersection(inner);
        if (!bar.empty()) {
            ctx.canvas.fill_rect(bar, detail::kFullBlock,
                                 Style{}.set_fg(options_.bar_color));
        }

        // 目标竖线：以行中心为轴上下各一格。
        const double tx = area_.x_scale().forward(std::clamp(r.target, 0.0, xmax + pad));
        ctx.canvas.draw_line(Point{tx, y - 1.0}, Point{tx, y + 1.0},
                             options_.target_char, target_st);

        if (!r.label.empty()) {
            ctx.canvas.draw_text(Point{0.0, y}, r.label, Style{});
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool BulletChart::to_scene(scene::Scene& out, std::uint32_t width,
                           std::uint32_t height) const {
    // 与 draw() 同源：剔除非有限行（value/target/三段带任一非有限即整行丢）。
    std::vector<Row> rows;
    rows.reserve(rows_.size());
    for (const Row& r : rows_) {
        bool ok = std::isfinite(r.value) && std::isfinite(r.target);
        for (const double bnd : r.bands) {
            ok = ok && std::isfinite(bnd);
        }
        if (ok) {
            rows.push_back(r);
        }
    }
    if (rows.empty()) {
        return true;
    }

    // x 定义域：所有带端点/度量/目标的最大值，底部从 0 起（与 draw 一致）。
    double xmax = 0.0;
    for (const Row& r : rows) {
        xmax = std::max(xmax, r.value);
        xmax = std::max(xmax, r.target);
        for (const double bnd : r.bands) {
            xmax = std::max(xmax, bnd);
        }
    }
    if (!(xmax > 0.0)) {
        xmax = 1.0;  // 全零数据仍画出三段带（都为 0 宽），避免零宽定义域
    }
    const double pad = xmax * 0.05;

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, 0.0, xmax + pad, 0.0,
                      static_cast<double>(rows.size()), false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());

    // 子弹图不画网格（与 draw 一致）；只画 x/y 轴框架，y 轴无刻度。
    SceneSink sink(out);
    layout::draw_axis_x(sink, area_, xticks, "", Style{});
    layout::draw_axis_y(sink, area_, {}, "", Style{});

    const Rect inner = area_.inner_rect();
    const Style target_st = Style{}.set_fg(default_color(DefaultColor::Warn));
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        const double y = area_.y_scale().forward(static_cast<double>(i) + 0.5);
        const Rect band_row{inner.x, y - 0.5, inner.w, 1.0};

        // 背景带：k 段从上一端点到本端点，统一裁进内框（实心色块近似原 ░▒▓ 明暗）。
        double lo = 0.0;
        for (std::size_t k = 0; k < r.bands.size(); ++k) {
            const double hi = std::max(lo, r.bands[k]);
            const double x0 = area_.x_scale().forward(lo);
            const double x1 = area_.x_scale().forward(hi);
            const Rect seg = Rect{x0, band_row.y, x1 - x0, band_row.h}.intersection(inner);
            if (!seg.empty()) {
                b.fill(default_color(DefaultColor::Muted));
                b.rect(seg);
            }
            lo = hi;
        }

        // 度量条：0 → value，实心块，盖在背景带之上。
        const double vx = area_.x_scale().forward(std::clamp(r.value, 0.0, xmax));
        const Rect bar = Rect{inner.x, y - 0.5, vx - inner.x, 1.0}.intersection(inner);
        if (!bar.empty()) {
            b.fill(options_.bar_color);
            b.rect(bar);
        }

        // 目标竖线：以行中心为轴上下各一格。
        const double tx = area_.x_scale().forward(std::clamp(r.target, 0.0, xmax + pad));
        b.stroke(target_st.fg());
        b.move_to(Point{tx, y - 1.0});
        b.line_to(Point{tx, y + 1.0});

        if (!r.label.empty()) {
            sink.text(Point{0.0, y}, r.label, Style{});
        }
    }
    b.end_group(0);

    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    return true;
}

}  // namespace termchart::chart
