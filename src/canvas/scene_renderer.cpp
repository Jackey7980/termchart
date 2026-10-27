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
//
// 这套"什么时候收口"的时序**不在本文件**——它在 scene::StatefulVisitor 里，
// canvas 与 SVG 后端共用同一份；本文件只回答"往哪画"。
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
#include "termchart/scene/stateful_visitor.hpp"

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

// 沿渐变轴采样颜色：linear 按投影比例 t、radial 按到圆心距离/半径。
Rgb sample_gradient(bool radial, const Point& g0, const Point& g1, double radius,
                    const std::vector<scene::GradientStop>& stops, double x, double y) {
    double t = 0.0;
    if (radial) {
        const double d = std::hypot(x - g0.x, y - g0.y);
        const double r = radius > 1e-9 ? radius : 1.0;
        t = std::clamp(d / r, 0.0, 1.0);
    } else {
        const double dx = g1.x - g0.x;
        const double dy = g1.y - g0.y;
        const double len2 = dx * dx + dy * dy;
        if (len2 < 1e-12) {
            t = 0.0;
        } else {
            t = std::clamp(((x - g0.x) * dx + (y - g0.y) * dy) / len2, 0.0, 1.0);
        }
    }
    if (stops.empty()) {
        return Rgb{0, 0, 0};
    }
    if (t <= stops.front().offset) {
        return stops.front().color;
    }
    if (t >= stops.back().offset) {
        return stops.back().color;
    }
    for (std::size_t i = 1; i < stops.size(); ++i) {
        if (t <= stops[i].offset) {
            const double span = stops[i].offset - stops[i - 1].offset;
            const double f = span < 1e-12 ? 0.0 : (t - stops[i - 1].offset) / span;
            const Rgb a = stops[i - 1].color;
            const Rgb b = stops[i].color;
            return Rgb{
                static_cast<std::uint8_t>(std::lround(a.r + (b.r - a.r) * f)),
                static_cast<std::uint8_t>(std::lround(a.g + (b.g - a.g) * f)),
                static_cast<std::uint8_t>(std::lround(a.b + (b.b - a.b) * f))};
        }
    }
    return stops.back().color;
}

class ScenePainter final : public scene::StatefulVisitor {
public:
    explicit ScenePainter(Canvas& canvas) noexcept
        : canvas_(&canvas),
          stroke_field_(canvas.width() * kStrokeSubX, canvas.height() * kStrokeSubY, 0.0f),
          fill_field_(canvas.width() * kFillSubX, canvas.height() * kFillSubY, 0.0f) {}

protected:
    // ── 路径 hooks ──
    void begin_path(const Point& /*from*/) override {
        // 终端侧不需要显式起点：extend_path 自带两端（a→b 整段都画）。
    }

    void extend_path(const Point& from, const Point& to) override { plot_segment(from, to); }

    void end_path() override {
        flush_stroke();  // dirty 守卫，多次调用幂等
    }

    // ── 图元 hooks ──
    void emit_filled_rect(const Rect& rect, const Color& fill) override {
        // 填充**即时**收口：一次 RectOp 就是一个完整的实心块，没有与别的图元
        // 合并的必要；且颜色已由参数给定，不必等到 end_path 再取状态。
        plot_filled_rect(rect);
        flush_fill(fill);
    }

    // 扩编指令（2026-10-26）：曲线折线化 + 渐变填充
    void extend_path_curve(const Point& from, const Point& c1, const Point& c2,
                          const Point& to) override {
        constexpr int kSteps = 24;
        Point prev = from;
        for (int i = 1; i <= kSteps; ++i) {
            const double t = static_cast<double>(i) / kSteps;
            const double u = 1.0 - t;
            const double x = u*u*u*from.x + 3.0*u*u*t*c1.x + 3.0*u*t*t*c2.x + t*t*t*to.x;
            const double y = u*u*u*from.y + 3.0*u*u*t*c1.y + 3.0*u*t*t*c2.y + t*t*t*to.y;
            plot_segment(prev, Point{x, y});
            prev = Point{x, y};
        }
    }

    void emit_filled_rect_gradient(const Rect& rect, bool radial, const Point& g0,
                                  const Point& g1, double radius,
                                  const std::vector<scene::GradientStop>& stops) override {
        const double x1 = rect.right();
        const double y1 = rect.bottom();
        for (double y = std::max(0.0, rect.y); y < y1; ++y) {
            for (double x = std::max(0.0, rect.x); x < x1; ++x) {
                if (!in_clip(x, y)) continue;
                const Rgb col = apply_alpha(sample_gradient(radial, g0, g1, radius, stops, x, y));
                canvas_->put(static_cast<std::uint32_t>(std::lround(x)),
                             static_cast<std::uint32_t>(std::lround(y)),
                             detail::kFullBlock, Style{}.set_fg(Color::rgb(col)));
            }
        }
    }

    void emit_stroked_rect(const Rect& rect, const Color& /*stroke*/, double /*width*/) override {
        // 描边与填充**反着来**：这里只往 stroke_field_ 累积、自己不 flush，
        // 真正的刷出在 end_path()（基类遇到状态指令 / 换图元 / run() 收尾时调）。
        // 原因：同一段描边色下可能连着来多个几何（矩形轮廓 + 若干 LineTo + 弧），
        // 逐个立即光栅化会把它们拆成多次 blit，既多开销又让叠加顺序不可控；
        // 累积到一次 end_path() 统一刷出，才与 SVG 的"一条 <path>"同构。
        // 代价：最后一个图元若是描边矩形，收尾那次 end_path() 是它唯一的刷出点
        // （见 StatefulVisitor::finish_path() 的注释）。
        // 线宽在终端侧无意义（一格就是一个字符），故忽略。
        plot_rect_outline(rect);
    }

    void emit_arc(const scene::ArcOp& arc, const Color& /*stroke*/, double /*width*/) override {
        plot_arc(arc);
    }

    void emit_text(const scene::TextOp& text) override {
        canvas_->draw_text(text.at, text.text, text.style);
    }

    void emit_image(const scene::ImageOp& image) override {
        // Scene 里没有像素数据（分层不允许 scene 依赖 image），这里只能是占位块。
        canvas_->fill_rect(image.rect, detail::kMediumShade, Style{});
    }

private:
    // ---- 描边：盲文 2×4 ----

    void plot_dot(double sx, double sy, bool thicken_vertically) {
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        if (!to_sub(sx, x) || !to_sub(sy, y)) {
            return;
        }
        // 裁剪：子像素坐标换算回用户坐标，跳过裁剪区外。
        if (!in_clip(sx / static_cast<double>(kStrokeSubX),
                     sy / static_cast<double>(kStrokeSubY))) {
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
        const auto emit = [this, thicken_vertically](double px, double py) {
            plot_dot(px, py, thicken_vertically);
        };
        if (steps <= 0) {
            emit(x0, y0);
            return;
        }
        const scene::DashOp* dash = current_dash();
        if (!dash) {
            for (int i = 0; i <= steps; ++i) {
                const double t = static_cast<double>(i) / static_cast<double>(steps);
                emit(x0 + dx * t, y0 + dy * t);
            }
            return;
        }
        // 虚线：逐采样点判定落在实段还是空段（dashes 偶索引=实、奇=空）。
        double pattern = 0.0;
        for (double d : dash->dashes) {
            pattern += d;
        }
        if (pattern <= 0.0) {
            return;
        }
        const double len = std::hypot(dx, dy);
        const double off = std::fmod(dash->offset, pattern) + (dash->offset < 0 ? pattern : 0.0);
        for (int i = 0; i <= steps; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(steps);
            const double along = t * len + off;
            const double m = std::fmod(along, pattern) + (along < 0 ? pattern : 0.0);
            double acc = 0.0;
            bool solid = true;
            for (std::size_t k = 0; k < dash->dashes.size(); ++k) {
                if (m < acc + dash->dashes[k]) {
                    solid = (k % 2 == 0);
                    break;
                }
                acc += dash->dashes[k];
            }
            if (solid) {
                emit(x0 + dx * t, y0 + dy * t);
            }
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
                if (!in_clip(sx, sy / static_cast<double>(kFillSubY))) {
                    continue;
                }
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
        opts.fg = apply_alpha_color(current_stroke());  // 旧值：状态指令是"先收口、后赋值"
        opts.bg = Color::none();     // 只画点，不动背景
        blit(raster::rasterize(stroke_field_, raster::RasterKind::Braille, opts));
        stroke_field_.fill(0.0f);
        stroke_dirty_ = false;
    }

    void flush_fill(const Color& fill) {
        if (!fill_dirty_) {
            return;
        }
        raster::RasterOptions opts;
        opts.fg = apply_alpha_color(fill);
        opts.bg = Color::none();
        blit(raster::rasterize(fill_field_, raster::RasterKind::HalfBlock, opts));
        fill_field_.fill(0.0f);
        fill_dirty_ = false;
    }

    Canvas* canvas_;
    image::FloatField stroke_field_;
    image::FloatField fill_field_;
    bool stroke_dirty_ = false;
    bool fill_dirty_ = false;

    // ── 扩编辅助 ──
    Rgb apply_alpha(Rgb c) const {
        const double a = current_alpha();
        if (a >= 1.0) {
            return c;
        }
        const auto sc = [a](std::uint8_t v) {
            return static_cast<std::uint8_t>(std::lround(v * a));
        };
        return Rgb{sc(c.r), sc(c.g), sc(c.b)};
    }
    Color apply_alpha_color(const Color& c) const {
        const double a = current_alpha();
        if (a >= 1.0 || c.kind() != ColorKind::Rgb) {
            return c;
        }
        const Rgb r = c.rgb();
        const auto sc = [a](std::uint8_t v) {
            return static_cast<std::uint8_t>(std::lround(v * a));
        };
        return Color::rgb(Rgb{sc(r.r), sc(r.g), sc(r.b)});
    }
    bool in_clip(double x, double y) const {
        const Rect* cr = clip_rect();
        if (!cr) {
            return true;
        }
        return x >= cr->x && y >= cr->y && x <= cr->right() && y <= cr->bottom();
    }
};

}  // namespace

void render_scene(const scene::Scene& scene, Canvas& canvas) noexcept {
    ScenePainter painter(canvas);
    painter.run(scene);
}

}  // namespace termchart::canvas
