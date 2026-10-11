// backend/vector/svg_backend.cpp —— Scene → SVG 实现
//
// 语义必须与权威消费者 canvas/scene_renderer.cpp 一致：FillOp/StrokeOp 是
// **状态指令，作用于其后出现的图元**。因此这里在遇到 Fill/Stroke 时**先**把
// 累积的折线用"旧状态"刷出，**再**更新状态；非路径图元（Rect/Arc/Text/Image）
// 用当前状态即时发出。任何"延迟到刷新时采用新状态"的做法都会让
// `几何 → Stroke` 这类顺序着色错位（那是真 bug）。
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

    std::string d;        // 待刷出的折线子路径（由 MoveTo/LineTo 累积）
    Point current{};      // 当前点（MoveTo 设置，LineTo 推进）
    bool subpath_open = false;
    Color fill = Color::none();
    Color stroke = Color::none();
    double stroke_w = 1.0;

    // 未显式设色的描边在终端里用"默认前景色"渲染 → SVG 用 currentColor 对应。
    const auto stroke_paint = [&stroke]() {
        return stroke.is_none() ? std::string("currentColor") : svg_color(stroke);
    };

    const auto flush = [&]() {
        if (d.empty()) {
            return;
        }
        out_ += "<path d=\"";
        out_ += d;
        out_ += "\" fill=\"none\" stroke=\"";
        out_ += stroke_paint();
        out_ += "\" stroke-width=\"";
        out_ += num(stroke_w);
        out_ += "\"/>";
        d.clear();
    };

    for (const scene::Command& cmd : scene.commands()) {
        std::visit(
            [&](const auto& op) {
                using T = std::decay_t<decltype(op)>;
                if constexpr (std::is_same_v<T, scene::MoveTo>) {
                    current = op.to;
                    subpath_open = false;  // 新子路径；真正开画要等 LineTo
                } else if constexpr (std::is_same_v<T, scene::LineTo>) {
                    if (!subpath_open) {
                        d += "M ";
                        d += num(current.x);
                        d += ' ';
                        d += num(current.y);
                        d += ' ';
                        subpath_open = true;
                    }
                    d += "L ";
                    d += num(op.to.x);
                    d += ' ';
                    d += num(op.to.y);
                    d += ' ';
                    current = op.to;
                } else if constexpr (std::is_same_v<T, scene::FillOp>) {
                    flush();  // 先按旧状态收口
                    fill = op.color;
                } else if constexpr (std::is_same_v<T, scene::StrokeOp>) {
                    flush();  // 先按旧状态收口
                    stroke = op.color;
                    stroke_w = op.width;
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
                    if (!fill.is_none()) {  // 与 canvas 一致：有填充色就填充
                        out_ += "\" fill=\"";
                        out_ += svg_color(fill);
                        out_ += "\" stroke=\"none\"/>";
                    } else {
                        out_ += "\" fill=\"none\" stroke=\"";
                        out_ += stroke_paint();
                        out_ += "\" stroke-width=\"";
                        out_ += num(stroke_w);
                        out_ += "\"/>";
                    }
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
                        out_ += "\" fill=\"none\" stroke=\"";
                        out_ += stroke_paint();
                        out_ += "\" stroke-width=\"";
                        out_ += num(stroke_w);
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
                        out_ += "\" fill=\"none\" stroke=\"";
                        out_ += stroke_paint();
                        out_ += "\" stroke-width=\"";
                        out_ += num(stroke_w);
                        out_ += "\"/>";
                    }
                } else if constexpr (std::is_same_v<T, scene::TextOp>) {
                    flush();
                    const std::string paint =
                        op.style.fg().is_none() ? std::string("currentColor")
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
