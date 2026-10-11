// backend/vector/svg_backend.cpp —— Scene → SVG 实现
//
// 路径状态机：MoveTo/LineTo 累积成 d；Fill/Stroke 只改状态；遇到非路径指令、
// 风格改变后的新几何、或结束时，把累积路径一次性落成 <path>（fill+stroke 同发）。
#include "termchart/backend/vector/svg_backend.hpp"

#include <cmath>
#include <string>
#include <type_traits>
#include <variant>

#include "termchart/backend/vector/svg_entities.hpp"
#include "termchart/scene/command.hpp"

namespace termchart::backend::vector {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

}  // namespace

void SvgBackend::render(const scene::Scene& scene) {
    out_.clear();
    const Rect b = scene.bounds();
    double vb_w = opts_.width;
    double vb_h = opts_.height;
    bool use_transform = false;
    double tx = 0.0;
    double ty = 0.0;
    if (opts_.autofit && b.w > 0.0 && b.h > 0.0) {
        vb_w = b.w * opts_.scale;
        vb_h = b.h * opts_.scale;
        tx = -b.x;
        ty = -b.y;
        use_transform = true;
    }

    out_ += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"";
    out_ += num(vb_w);
    out_ += "\" height=\"";
    out_ += num(vb_h);
    out_ += "\" viewBox=\"0 0 ";
    out_ += num(vb_w);
    out_ += ' ';
    out_ += num(vb_h);
    out_ += "\">";
    if (use_transform) {
        out_ += "<g transform=\"scale(";
        out_ += num(opts_.scale);
        out_ += ") translate(";
        out_ += num(tx);
        out_ += ' ';
        out_ += num(ty);
        out_ += ")\">";
    }

    std::string d;
    Color fill = Color::none();
    Color stroke = Color::none();
    double stroke_w = 1.0;
    bool dirty = false;
    bool style_dirty = false;

    const auto flush = [&]() {
        if (!dirty || d.empty()) {
            dirty = false;
            style_dirty = false;
            return;
        }
        if (fill.is_none() && stroke.is_none()) {  // 无 paint 的几何不输出
            d.clear();
            dirty = false;
            style_dirty = false;
            return;
        }
        out_ += "<path d=\"";
        out_ += d;
        out_ += "\" fill=\"";
        out_ += svg_color(fill);
        out_ += "\" stroke=\"";
        out_ += svg_color(stroke);
        out_ += "\" stroke-width=\"";
        out_ += num(stroke_w);
        out_ += "\"/>";
        d.clear();
        dirty = false;
        style_dirty = false;
    };

    for (const scene::Command& cmd : scene.commands()) {
        std::visit(
            [&](const auto& op) {
                using T = std::decay_t<decltype(op)>;
                if constexpr (std::is_same_v<T, scene::MoveTo>) {
                    if (style_dirty) {
                        flush();
                    }
                    d += "M ";
                    d += num(op.to.x);
                    d += ' ';
                    d += num(op.to.y);
                    d += ' ';
                    dirty = true;
                } else if constexpr (std::is_same_v<T, scene::LineTo>) {
                    if (style_dirty) {
                        flush();
                    }
                    d += "L ";
                    d += num(op.to.x);
                    d += ' ';
                    d += num(op.to.y);
                    d += ' ';
                    dirty = true;
                } else if constexpr (std::is_same_v<T, scene::FillOp>) {
                    fill = op.color;
                    style_dirty = true;
                } else if constexpr (std::is_same_v<T, scene::StrokeOp>) {
                    stroke = op.color;
                    stroke_w = op.width;
                    style_dirty = true;
                } else if constexpr (std::is_same_v<T, scene::RectOp>) {
                    flush();
                    out_ += "<rect x=\"";
                    out_ += num(op.rect.x);
                    out_ += "\" y=\"";
                    out_ += num(op.rect.y);
                    out_ += "\" width=\"";
                    out_ += num(op.rect.w);
                    out_ += "\" height=\"";
                    out_ += num(op.rect.h);
                    out_ += "\" fill=\"";
                    out_ += svg_color(fill);
                    out_ += "\" stroke=\"";
                    out_ += svg_color(stroke);
                    out_ += "\"/>";
                } else if constexpr (std::is_same_v<T, scene::ArcOp>) {
                    flush();
                    const double span = op.end - op.start;
                    if (span >= kTwoPi - 1e-9) {  // 整圆
                        out_ += "<circle cx=\"";
                        out_ += num(op.center.x);
                        out_ += "\" cy=\"";
                        out_ += num(op.center.y);
                        out_ += "\" r=\"";
                        out_ += num(op.radius);
                        out_ += "\" fill=\"";
                        out_ += svg_color(fill);
                        out_ += "\" stroke=\"";
                        out_ += svg_color(stroke);
                        out_ += "\"/>";
                    } else {
                        const double x0 = op.center.x + op.radius * std::cos(op.start);
                        const double y0 = op.center.y + op.radius * std::sin(op.start);
                        const double x1 = op.center.x + op.radius * std::cos(op.end);
                        const double y1 = op.center.y + op.radius * std::sin(op.end);
                        out_ += "<path d=\"M ";
                        out_ += num(x0);
                        out_ += ' ';
                        out_ += num(y0);
                        out_ += " A ";
                        out_ += num(op.radius);
                        out_ += ' ';
                        out_ += num(op.radius);
                        out_ += " 0 ";
                        out_ += (span > kPi ? '1' : '0');
                        out_ += " 1 ";
                        out_ += num(x1);
                        out_ += ' ';
                        out_ += num(y1);
                        out_ += "\" fill=\"";
                        out_ += svg_color(fill);
                        out_ += "\" stroke=\"";
                        out_ += svg_color(stroke);
                        out_ += "\"/>";
                    }
                } else if constexpr (std::is_same_v<T, scene::TextOp>) {
                    flush();
                    const std::string paint =
                        op.style.fg().is_none() ? std::string("black")
                                                : svg_color(op.style.fg());
                    out_ += "<text x=\"";
                    out_ += num(op.at.x);
                    out_ += "\" y=\"";
                    out_ += num(op.at.y);
                    out_ += "\" font-family=\"monospace\" font-size=\"";
                    out_ += num(opts_.scale);
                    out_ += "\" dominant-baseline=\"text-before-edge\" fill=\"";
                    out_ += paint;
                    out_ += "\">";
                    out_ += xml_escape(op.text);
                    out_ += "</text>";
                } else if constexpr (std::is_same_v<T, scene::ImageOp>) {
                    flush();
                    out_ += "<rect x=\"";
                    out_ += num(op.rect.x);
                    out_ += "\" y=\"";
                    out_ += num(op.rect.y);
                    out_ += "\" width=\"";
                    out_ += num(op.rect.w);
                    out_ += "\" height=\"";
                    out_ += num(op.rect.h);
                    out_ += "\" fill=\"none\" stroke=\"#888888\" data-image-id=\"";
                    out_ += std::to_string(op.image_id);
                    out_ += "\"/>";
                }
            },
            cmd);
    }
    flush();

    if (use_transform) {
        out_ += "</g>";
    }
    out_ += "</svg>";
}

}  // namespace termchart::backend::vector
