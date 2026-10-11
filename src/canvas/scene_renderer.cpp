// scene_renderer.cpp —— Scene → Canvas 的唯一桥接实现（R12：已接 raster）
//
// 取字符策略（这里曾经是 kStrokeChar = '*' 的占位实现，现改为真子像素）：
//   * 描边（LineTo / ArcOp / RectOp 轮廓）→ 累积进 **braille 2×4** 子像素场，
//     按当前 StrokeOp 的颜色光栅化。盲文分辨率最高，曲线最连续。
//   * 填充（FillOp 之后的 RectOp）→ 累积进 **halfblock 1×2** 子像素场，整格实心。
//   * 文本仍走 canvas.draw_text（文本不是几何）；ImageOp 仍是占位（Scene 里没有像素）。
//
// 状态语义与 backend/vector 的 SVG 后端严格同构：Fill/Stroke 只作用于**其后**出现的
// 图元，所以遇到状态指令时先把已累积的几何按**旧颜色**收口，再更新状态。
#include "termchart/canvas/scene_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/primitives.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/detail/block_chars.hpp"
#include "termchart/image/field.hpp"
#include "termchart/raster/rasterizer.hpp"
#include "termchart/scene/visitor.hpp"

namespace termchart::canvas {
namespace {

// 子像素密度：描边用盲文（2×4），填充用半块（1×2）。与 raster/ 的约定一致。
constexpr std::uint32_t kStrokeSubX = 2;
constexpr std::uint32_t kStrokeSubY = 4;
constexpr std::uint32_t kFillSubX = 1;
constexpr std::uint32_t kFillSubY = 2;

// 光栅器产出的"空"不只是空格：盲文空白是 U+2800（BRAILLE PATTERN BLANK）。
// 两者都必须跳过，否则一次收口会用空白整片覆盖画布，把先前画好的内容擦掉
// （实测：先画折线、再画圆弧时，折线会被圆弧那次收口的空白格抹掉）。
bool is_empty_glyph(char32_t cp) noexcept {
    return cp == U' ' || cp == U'\u2800';
}

// 浮点子像素坐标 → 下标；负数/极大值一律拒绝（不折回、不越界）。
bool to_sub(double value, std::uint32_t& out) noexcept {
    if (!(value >= 0.0) || value > 1.0e9) {
        return false;
    }
    out = static_cast<std::uint32_t>(std::lround(value));
    return true;
}

class ScenePainter final : public scene::CommandVisitor {
public:
    explicit ScenePainter(Canvas& canvas) noexcept
        : canvas_(&canvas),
          stroke_field_(canvas.width() * kStrokeSubX, canvas.height() * kStrokeSubY, 0.0f),
          fill_field_(canvas.width() * kFillSubX, canvas.height() * kFillSubY, 0.0f) {}

    void visit(const scene::MoveTo& command) override { current_ = command.to; }

    void visit(const scene::LineTo& command) override {
        plot_segment(current_, command.to);
        current_ = command.to;
    }

    void visit(const scene::RectOp& command) override {
        if (!fill_.fg().is_none()) {
            plot_filled_rect(command.rect);  // 有填充色 → 实心
            return;
        }
        plot_rect_outline(command.rect);  // 否则描边（与旧行为一致）
    }

    void visit(const scene::ArcOp& command) override { plot_arc(command); }

    // Fill / Stroke 是**状态指令**：先按旧状态收口，再更新状态。
    void visit(const scene::FillOp& command) override {
        flush();
        fill_ = Style{}.set_fg(command.color);
    }

    void visit(const scene::StrokeOp& command) override {
        flush();
        stroke_ = Style{}.set_fg(command.color);
    }

    void visit(const scene::TextOp& command) override {
        canvas_->draw_text(command.at, command.text, command.style);
    }

    void visit(const scene::ImageOp& command) override {
        // Scene 里没有像素数据（分层不允许 scene 依赖 image），这里只能是占位块。
        canvas_->fill_rect(command.rect, detail::kMediumShade, Style{});
    }

    // 收口：先填充后描边，轮廓压在填充之上。
    void flush() {
        flush_fill();
        flush_stroke();
    }

private:
    // ---- 描边：盲文 2×4 ----

    void plot_dot(double sx, double sy, bool thicken_vertically) {
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        if (!to_sub(sx, x) || !to_sub(sy, y)) {
            return;
        }
        stroke_field_.set(x, y, 1.0f);
        // 加粗一格子像素：横线加高、竖线加宽。单点宽的线在盲文密度下会断成散点。
        if (thicken_vertically) {
            stroke_field_.set(x, y + 1, 1.0f);
        } else {
            stroke_field_.set(x + 1, y, 1.0f);
        }
        stroke_dirty_ = true;
    }

    void plot_segment(Point a, Point b) {
        const double x0 = a.x * static_cast<double>(kStrokeSubX);
        const double y0 = a.y * static_cast<double>(kStrokeSubY);
        const double dx = (b.x - a.x) * static_cast<double>(kStrokeSubX);
        const double dy = (b.y - a.y) * static_cast<double>(kStrokeSubY);
        const int steps = static_cast<int>(std::ceil(std::max(std::fabs(dx), std::fabs(dy))));
        const bool thicken_vertically = std::fabs(dx) >= std::fabs(dy);  // 偏横 → 加高
        if (steps <= 0) {
            plot_dot(x0, y0, thicken_vertically);
            return;
        }
        for (int i = 0; i <= steps; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(steps);
            plot_dot(x0 + dx * t, y0 + dy * t, thicken_vertically);
        }
    }

    void plot_rect_outline(Rect rect) {
        const Point tl{rect.x, rect.y};
        const Point tr{rect.right(), rect.y};
        const Point br{rect.right(), rect.bottom()};
        const Point bl{rect.x, rect.bottom()};
        plot_segment(tl, tr);
        plot_segment(tr, br);
        plot_segment(br, bl);
        plot_segment(bl, tl);
    }

    void plot_arc(const scene::ArcOp& arc) {
        const double span = arc.end - arc.start;
        // 采样步数按弧长（换算到子像素）取，保证相邻采样点之间不会漏格。
        const double radius_sub = arc.radius * static_cast<double>(kStrokeSubY);
        const int steps = std::max(
            8, static_cast<int>(std::ceil(std::fabs(span) * std::max(1.0, radius_sub))));
        Point prev{};
        for (int i = 0; i <= steps; ++i) {
            const double angle = arc.start + span * static_cast<double>(i) /
                                                 static_cast<double>(steps);
            const Point p{arc.center.x + arc.radius * std::cos(angle),
                          arc.center.y + arc.radius * std::sin(angle)};
            if (i == 0) {
                plot_dot(p.x * kStrokeSubX, p.y * kStrokeSubY, false);
            } else {
                plot_segment(prev, p);
            }
            prev = p;
        }
    }

    // ---- 填充：半块 1×2 ----

    void plot_filled_rect(Rect rect) {
        if (!(rect.w > 0.0) || !(rect.h > 0.0)) {
            return;
        }
        const double x0 = std::max(0.0, rect.x);
        const double x1 = rect.right();
        const double y0 = std::max(0.0, rect.y * static_cast<double>(kFillSubY));
        const double y1 = rect.bottom() * static_cast<double>(kFillSubY);
        for (double sy = y0; sy < y1; sy += 1.0) {
            for (double sx = x0; sx < x1; sx += 1.0) {
                std::uint32_t ux = 0;
                std::uint32_t uy = 0;
                if (!to_sub(sx, ux) || !to_sub(sy, uy)) {
                    continue;
                }
                fill_field_.set(ux, uy, 1.0f);
                fill_dirty_ = true;
            }
        }
    }

    // ---- 收口与叠加 ----

    // 只覆盖非空格：空白格不擦除画布上已有内容（同一画布可叠加多个 Scene）。
    void blit(const CellBuffer& buffer) {
        for (std::uint32_t y = 0; y < buffer.height(); ++y) {
            for (std::uint32_t x = 0; x < buffer.width(); ++x) {
                const Cell& cell = buffer.at(x, y);
                if (is_empty_glyph(cell.cp) || is_continuation(cell)) {
                    continue;
                }
                canvas_->put(x, y, cell.cp, cell.style);
            }
        }
    }

    void flush_stroke() {
        if (!stroke_dirty_) {
            return;
        }
        raster::RasterOptions opts;
        opts.fg = stroke_.fg();
        opts.bg = Color::none();  // 只画点，不动背景
        blit(raster::rasterize(stroke_field_, raster::RasterKind::Braille, opts));
        stroke_field_.fill(0.0f);
        stroke_dirty_ = false;
    }

    void flush_fill() {
        if (!fill_dirty_) {
            return;
        }
        raster::RasterOptions opts;
        opts.fg = fill_.fg();
        opts.bg = Color::none();
        blit(raster::rasterize(fill_field_, raster::RasterKind::HalfBlock, opts));
        fill_field_.fill(0.0f);
        fill_dirty_ = false;
    }

    Canvas* canvas_;
    Point current_{};
    Style fill_{};
    Style stroke_{};
    image::FloatField stroke_field_;
    image::FloatField fill_field_;
    bool stroke_dirty_ = false;
    bool fill_dirty_ = false;
};

}  // namespace

void render_scene(const scene::Scene& scene, Canvas& canvas) noexcept {
    ScenePainter painter(canvas);
    scene::accept_all(scene, painter);
    painter.flush();
}

}  // namespace termchart::canvas
