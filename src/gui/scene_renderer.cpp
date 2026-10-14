// scene_renderer.cpp —— Scene → 位图渲染核心
//
// 三段式：换算变换 → 超采样几何 → 降采样 + 文本直绘。
// 时序全部来自 scene::StatefulVisitor，本文件不写任何"什么时候画"。
#include "termchart/gui/scene_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "termchart/core/rect.hpp"
#include "termchart/gui/font.hpp"

namespace termchart::gui {
namespace {

constexpr double kTwoPi = 6.28318530717958647692;

std::uint8_t clamp_byte(double value) noexcept {
    if (!(value > 0.0)) {
        return 0;
    }
    if (value > 255.0) {
        return 255;
    }
    return static_cast<std::uint8_t>(value + 0.5);
}

}  // namespace

SceneRenderer::SceneRenderer(RenderOptions options) noexcept : opts_(options) {
    if (opts_.supersample < 1) {
        opts_.supersample = 1;
    }
    if (opts_.supersample > 4) {
        opts_.supersample = 4;
    }
    resolve_.default_fg = opts_.foreground;
    resolve_.default_bg = opts_.background;
}

double SceneRenderer::to_px_x(double x) const noexcept {
    return (x * view_scale_ + view_tx_) * static_cast<double>(ss_);
}

double SceneRenderer::to_px_y(double y) const noexcept {
    return (y * view_scale_ + view_ty_) * static_cast<double>(ss_);
}

image::Bitmap SceneRenderer::render(const scene::Scene& scene) {
    // ── 1. 几何变换：autofit 与 SVG 后端同构（按 bounds 缩放 + 边距平移）──
    ss_ = opts_.supersample;
    view_scale_ = opts_.scale;
    view_tx_ = 0.0;
    view_ty_ = 0.0;
    if (opts_.autofit && opts_.width > 0 && opts_.height > 0) {
        const Rect b = scene.bounds();
        if (b.w > 0.0 && b.h > 0.0) {
            const double avail_w =
                std::max(1.0, static_cast<double>(opts_.width) - 2.0 * opts_.margin);
            const double avail_h =
                std::max(1.0, static_cast<double>(opts_.height) - 2.0 * opts_.margin);
            view_scale_ = std::min(avail_w / b.w, avail_h / b.h);
            view_tx_ = opts_.margin - b.x * view_scale_;
            view_ty_ = opts_.margin - b.y * view_scale_;
        }
    }

    sw_ = opts_.width * static_cast<std::uint32_t>(ss_);
    sh_ = opts_.height * static_cast<std::uint32_t>(ss_);
    buffer_.assign(static_cast<std::size_t>(sw_) * sh_ * 4, 0);
    if (!opts_.transparent) {  // 背景：整块填（含 alpha 255）
        for (std::size_t i = 0; i < buffer_.size(); i += 4) {
            buffer_[i + 0] = opts_.background.r;
            buffer_[i + 1] = opts_.background.g;
            buffer_[i + 2] = opts_.background.b;
            buffer_[i + 3] = 255;
        }
    }

    texts_.clear();

    // ── 2. 几何：时序在基类，这里只往超采样缓冲落盘 ──
    run(scene);

    // ── 3a. 盒式降采样：ss×ss 取平均 = 免费抗锯齿 ──
    image::Bitmap out(opts_.width, opts_.height, 4);
    const double inv = 1.0 / static_cast<double>(ss_ * ss_);
    for (std::uint32_t py = 0; py < opts_.height; ++py) {
        for (std::uint32_t px = 0; px < opts_.width; ++px) {
            double r = 0.0;
            double g = 0.0;
            double b = 0.0;
            double a = 0.0;
            for (int sy = 0; sy < ss_; ++sy) {
                const std::size_t row =
                    (static_cast<std::size_t>(py) * static_cast<std::size_t>(ss_) +
                     static_cast<std::size_t>(sy)) *
                    static_cast<std::size_t>(sw_) * 4;
                for (int sx = 0; sx < ss_; ++sx) {
                    const std::size_t idx =
                        row + (static_cast<std::size_t>(px) * static_cast<std::size_t>(ss_) +
                               static_cast<std::size_t>(sx)) *
                                  4;
                    r += buffer_[idx + 0];
                    g += buffer_[idx + 1];
                    b += buffer_[idx + 2];
                    a += buffer_[idx + 3];
                }
            }
            out.set(px, py, 0, clamp_byte(r * inv));
            out.set(px, py, 1, clamp_byte(g * inv));
            out.set(px, py, 2, clamp_byte(b * inv));
            out.set(px, py, 3, clamp_byte(a * inv));
        }
    }

    // ── 3b. 文本直绘（最终分辨率，永远在几何之上）──
    for (const PendingText& t : texts_) {
        // 布局坐标已在 Scene/RenderOptions 里定死，advance 在此无消费方——显式丢弃。
        static_cast<void>(draw_text(out, t.text, Point{t.x, t.y}, t.color, t.cell_w, t.cell_h));
    }

    buffer_.clear();
    buffer_.shrink_to_fit();
    return out;
}

// ── 路径 hooks ──────────────────────────────────────────────────────────────

void SceneRenderer::begin_path(const Point& /*from*/) {
    // 起点即首段的 from，无需单独落盘（extend_path 自带两端）。
}

void SceneRenderer::extend_path(const Point& from, const Point& to) {
    // 颜色与线宽在此刻读取：时序保证 Fill/Stroke 已"先收口后赋值"，
    // 因此 current_* 恰好是这段几何出现时的状态。
    const Rgb rgb = resolve_color(current_stroke(), resolve_);
    const double radius =
        std::max(0.5, 0.5 * current_stroke_width() * view_scale_ * static_cast<double>(ss_));
    draw_segment(to_px_x(from.x), to_px_y(from.y), to_px_x(to.x), to_px_y(to.y), radius, rgb);
}

void SceneRenderer::end_path() {
    // 几何已即时落盘，无需收尾（幂等空实现——基类约定允许）。
}

// ── 图元 hooks ──────────────────────────────────────────────────────────────

void SceneRenderer::emit_filled_rect(const Rect& rect, const Color& fill) {
    if (!(rect.w > 0.0) || !(rect.h > 0.0)) {
        return;
    }
    fill_rect_ss(rect.x, rect.y, rect.w, rect.h, resolve_color(fill, resolve_));
}

void SceneRenderer::emit_stroked_rect(const Rect& rect, const Color& /*stroke*/, double width) {
    if (!(rect.w > 0.0) || !(rect.h > 0.0)) {
        return;
    }
    stroke_rect_ss(rect, width, resolve_color(current_stroke(), resolve_));
}

void SceneRenderer::emit_arc(const scene::ArcOp& arc, const Color& /*stroke*/, double width) {
    const Rgb rgb = resolve_color(current_stroke(), resolve_);
    const double radius =
        std::max(0.5, 0.5 * width * view_scale_ * static_cast<double>(ss_));
    const double span = arc.end - arc.start;
    const double r_ss = std::max(2.0, arc.radius * view_scale_ * static_cast<double>(ss_));
    // 步数按弧长折算：相邻 stamp 间距不超过 ~0.5 超采样像素，曲线不断线。
    const int steps = std::clamp(
        static_cast<int>(std::ceil(std::fabs(span) * std::max(6.0, r_ss / 2.0))), 24, 4096);
    double prev_x = 0.0;
    double prev_y = 0.0;
    for (int i = 0; i <= steps; ++i) {
        const double angle =
            arc.start + span * static_cast<double>(i) / static_cast<double>(steps);
        const double px = to_px_x(arc.center.x + arc.radius * std::cos(angle));
        const double py = to_px_y(arc.center.y + arc.radius * std::sin(angle));
        if (i > 0) {
            draw_segment(prev_x, prev_y, px, py, radius, rgb);
        } else {
            stamp(px, py, radius, rgb);  // 圆头起点
        }
        prev_x = px;
        prev_y = py;
    }
}

void SceneRenderer::emit_text(const scene::TextOp& text) {
    PendingText t;
    t.x = to_px_x(text.at.x) / static_cast<double>(ss_);
    t.y = to_px_y(text.at.y) / static_cast<double>(ss_);
    t.cell_w = opts_.text_cell_w * view_scale_;
    t.cell_h = opts_.text_cell_h * view_scale_;
    t.color = resolve_color(text.style.fg(), resolve_);
    t.text = text.text;
    texts_.push_back(std::move(t));
}

void SceneRenderer::emit_image(const scene::ImageOp& image) {
    // Scene 不持有像素（分层铁律），画"棋盘占位 + 边框"标出图的位置与尺寸。
    const Rect& rect = image.rect;
    if (!(rect.w > 0.0) || !(rect.h > 0.0)) {
        return;
    }
    fill_rect_ss(rect.x, rect.y, rect.w, rect.h, Rgb{38, 38, 38});
    const double cell = 8.0 * view_scale_;  // 8 场景单位一格，与 text_cell 对齐量级
    const std::uint32_t cols = static_cast<std::uint32_t>(std::ceil(rect.w / cell));
    const std::uint32_t rows = static_cast<std::uint32_t>(std::ceil(rect.h / cell));
    for (std::uint32_t cy = 0; cy < rows; ++cy) {
        for (std::uint32_t cx = 0; cx < cols; ++cx) {
            if ((cx + cy) % 2 == 0) {
                const double x = rect.x + static_cast<double>(cx) * cell;
                const double y = rect.y + static_cast<double>(cy) * cell;
                fill_rect_ss(x, y, std::min(cell, rect.right() - x),
                             std::min(cell, rect.bottom() - y), Rgb{56, 56, 56});
            }
        }
    }
    stroke_rect_ss(rect, 1.0, Rgb{128, 128, 128});
}

// ── 超采样原语 ──────────────────────────────────────────────────────────────

void SceneRenderer::stamp(double cx, double cy, double radius, const Rgb& rgb) {
    const double r2 = radius * radius;
    const int x0 = static_cast<int>(std::floor(cx - radius));
    const int x1 = static_cast<int>(std::ceil(cx + radius));
    const int y0 = static_cast<int>(std::floor(cy - radius));
    const int y1 = static_cast<int>(std::ceil(cy + radius));
    for (int y = y0; y <= y1; ++y) {
        if (y < 0 || y >= static_cast<int>(sh_)) {
            continue;
        }
        const double dy = static_cast<double>(y) + 0.5 - cy;
        for (int x = x0; x <= x1; ++x) {
            if (x < 0 || x >= static_cast<int>(sw_)) {
                continue;
            }
            const double dx = static_cast<double>(x) + 0.5 - cx;
            if (dx * dx + dy * dy <= r2) {
                const std::size_t idx =
                    (static_cast<std::size_t>(y) * sw_ + static_cast<std::size_t>(x)) * 4;
                buffer_[idx + 0] = rgb.r;
                buffer_[idx + 1] = rgb.g;
                buffer_[idx + 2] = rgb.b;
                buffer_[idx + 3] = 255;
            }
        }
    }
}

void SceneRenderer::draw_segment(double x0, double y0, double x1, double y1, double radius,
                                 const Rgb& rgb) {
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double len = std::sqrt(dx * dx + dy * dy);
    // 采样密度 0.5 超采样像素：stamp 间距不超过半径的一半即可保证连续。
    const int steps = std::max(1, static_cast<int>(std::ceil(len * 2.0)));
    for (int i = 0; i <= steps; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(steps);
        stamp(x0 + dx * t, y0 + dy * t, radius, rgb);
    }
}

void SceneRenderer::fill_rect_ss(double x, double y, double w, double h, const Rgb& rgb) {
    if (!(w > 0.0) || !(h > 0.0)) {
        return;
    }
    const double px0 = to_px_x(x);
    const double px1 = to_px_x(x + w);
    const double py0 = to_px_y(y);
    const double py1 = to_px_y(y + h);
    const int ix0 = std::max(0, static_cast<int>(std::floor(px0)));
    const int ix1 = std::min(static_cast<int>(sw_), static_cast<int>(std::ceil(px1)));
    const int iy0 = std::max(0, static_cast<int>(std::floor(py0)));
    const int iy1 = std::min(static_cast<int>(sh_), static_cast<int>(std::ceil(py1)));
    for (int iy = iy0; iy < iy1; ++iy) {
        const std::size_t row = static_cast<std::size_t>(iy) * sw_ * 4;
        for (int ix = ix0; ix < ix1; ++ix) {
            const std::size_t idx = row + static_cast<std::size_t>(ix) * 4;
            buffer_[idx + 0] = rgb.r;
            buffer_[idx + 1] = rgb.g;
            buffer_[idx + 2] = rgb.b;
            buffer_[idx + 3] = 255;
        }
    }
}

void SceneRenderer::stroke_rect_ss(const Rect& rect, double width, const Rgb& rgb) {
    const double radius = std::max(0.5, 0.5 * width * view_scale_ * static_cast<double>(ss_));
    const double x0 = to_px_x(rect.x);
    const double x1 = to_px_x(rect.right());
    const double y0 = to_px_y(rect.y);
    const double y1 = to_px_y(rect.bottom());
    draw_segment(x0, y0, x1, y0, radius, rgb);
    draw_segment(x1, y0, x1, y1, radius, rgb);
    draw_segment(x1, y1, x0, y1, radius, rgb);
    draw_segment(x0, y1, x0, y0, radius, rgb);
}

}  // namespace termchart::gui
