// backend/vector/svg_backend.cpp —— Scene → SVG 实现
//
// 语义必须与权威消费者 canvas/scene_renderer.cpp 一致：FillOp/StrokeOp 是
// **状态指令，作用于其后出现的图元**。因此这里在遇到 Fill/Stroke 时**先**把
// 累积的折线用"旧状态"刷出，**再**更新状态；非路径图元（Rect/Arc/Text/Image）
// 用当前状态即时发出。任何"延迟到刷新时采用新状态"的做法都会让
// `几何 → Stroke` 这类顺序着色错位（那是真 bug）。
//
// 这套时序由 scene::StatefulVisitor 统一提供（canvas 侧共用同一份），
// 本文件只负责"往 SVG 字符串里写什么"。
#include "termchart/backend/vector/svg_backend.hpp"

#include <cmath>
#include <memory>
#include <string>

#include "termchart/backend/vector/svg_entities.hpp"
#include "termchart/media/image/ttf_font.hpp"
#include "termchart/scene/stateful_visitor.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::backend::vector {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

// 未显式设色的描边在终端里用"默认前景色"渲染 → SVG 用 currentColor 对应。
std::string paint_of(const Color& color) {
    return color.is_none() ? std::string("currentColor") : svg_color(color);
}

class SvgVisitor final : public scene::StatefulVisitor {
public:
    SvgVisitor(std::string& out, const VectorOptions& opts, const image::TtfFont* font) noexcept
        : out_(&out), opts_(&opts), font_(font) {}

protected:
    // ── 路径 hooks ──
    void begin_path(const Point& from) override {
        d_ += "M ";
        d_ += num(from.x);
        d_ += ' ';
        d_ += num(from.y);
        d_ += ' ';
    }

    void extend_path(const Point& /*from*/, const Point& to) override {
        d_ += "L ";
        d_ += num(to.x);
        d_ += ' ';
        d_ += num(to.y);
        d_ += ' ';
    }

    void end_path() override {
        if (d_.empty()) {
            return;
        }
        *out_ += "<path d=\"";
        *out_ += d_;
        *out_ += "\" fill=\"none\" stroke=\"";
        *out_ += paint_of(current_stroke());
        *out_ += "\" stroke-width=\"";
        *out_ += num(current_stroke_width());
        *out_ += "\"/>";
        d_.clear();
    }

    // ── 图元 hooks ──
    void emit_filled_rect(const Rect& rect, const Color& fill) override {
        *out_ += "<rect x=\"";
        *out_ += num(rect.x);
        *out_ += "\" y=\"";
        *out_ += num(rect.y);
        *out_ += "\" width=\"";
        *out_ += num(rect.w);
        *out_ += "\" height=\"";
        *out_ += num(rect.h);
        *out_ += "\" fill=\"";
        *out_ += svg_color(fill);
        *out_ += "\" stroke=\"none\"/>";
    }

    void emit_stroked_rect(const Rect& rect, const Color& stroke, double width) override {
        *out_ += "<rect x=\"";
        *out_ += num(rect.x);
        *out_ += "\" y=\"";
        *out_ += num(rect.y);
        *out_ += "\" width=\"";
        *out_ += num(rect.w);
        *out_ += "\" height=\"";
        *out_ += num(rect.h);
        *out_ += "\" fill=\"none\" stroke=\"";
        *out_ += paint_of(stroke);
        *out_ += "\" stroke-width=\"";
        *out_ += num(width);
        *out_ += "\"/>";
    }

    void emit_arc(const scene::ArcOp& arc, const Color& stroke, double width) override {
        const std::string sp = paint_of(stroke);
        const double span = arc.end - arc.start;
        if (span >= kTwoPi - 1e-9) {  // 整圆
            *out_ += "<circle cx=\"";
            *out_ += num(arc.center.x);
            *out_ += "\" cy=\"";
            *out_ += num(arc.center.y);
            *out_ += "\" r=\"";
            *out_ += num(arc.radius);
            *out_ += "\" fill=\"none\" stroke=\"";
            *out_ += sp;
            *out_ += "\" stroke-width=\"";
            *out_ += num(width);
            *out_ += "\"/>";
            return;
        }
        const double x0 = arc.center.x + arc.radius * std::cos(arc.start);
        const double y0 = arc.center.y + arc.radius * std::sin(arc.start);
        const double x1 = arc.center.x + arc.radius * std::cos(arc.end);
        const double y1 = arc.center.y + arc.radius * std::sin(arc.end);
        *out_ += "<path d=\"M ";
        *out_ += num(x0);
        *out_ += ' ';
        *out_ += num(y0);
        *out_ += " A ";
        *out_ += num(arc.radius);
        *out_ += ' ';
        *out_ += num(arc.radius);
        *out_ += " 0 ";
        *out_ += (span > kPi ? '1' : '0');
        *out_ += " 1 ";
        *out_ += num(x1);
        *out_ += ' ';
        *out_ += num(y1);
        *out_ += "\" fill=\"none\" stroke=\"";
        *out_ += sp;
        *out_ += "\" stroke-width=\"";
        *out_ += num(width);
        *out_ += "\"/>";
    }

    void emit_text(const scene::TextOp& text) override {
        // 文字轮廓化：把每个字形转成自包含矢量路径（不再依赖渲染端字体）。
        if (font_ != nullptr) {
            const double px = opts_->font_size > 0.0 ? opts_->font_size : opts_->scale;
            const double baseline = text.at.y + font_->metrics(px).ascent;
            const std::string fill = paint_of(text.style.fg());
            double pen_x = text.at.x;
            std::size_t pos = 0;
            while (pos < text.text.size()) {
                const auto dec = util::decode(text.text, pos);
                pos = static_cast<std::size_t>(dec.consumed);
                const auto go = font_->glyph_outline(dec.cp, px);
                if (go && !go->contours.empty()) {
                    for (const auto& contour : go->contours) {
                        *out_ += "<path d=\"M ";
                        bool first = true;
                        for (const auto& p : contour) {
                            if (!first) {
                                *out_ += ' ';
                            }
                            *out_ += num(pen_x + p.x);
                            *out_ += ' ';
                            *out_ += num(baseline + p.y);
                            first = false;
                        }
                        *out_ += " Z\" fill=\"";
                        *out_ += fill;
                        *out_ += "\" stroke=\"none\"/>";
                    }
                }
                if (go) {
                    pen_x += go->advance_x;
                } else {
                    pen_x += px * 0.5;  // 缺字形兜底步进
                }
            }
            return;
        }
        // 旧行为：<text>
        *out_ += "<text x=\"";
        *out_ += num(text.at.x);
        *out_ += "\" y=\"";
        *out_ += num(text.at.y);
        *out_ += "\" font-family=\"monospace\" font-size=\"";
        *out_ += num(opts_->font_size > 0.0 ? opts_->font_size : opts_->scale);
        *out_ += "\" dominant-baseline=\"text-before-edge\" fill=\"";
        *out_ += paint_of(text.style.fg());
        *out_ += "\">";
        *out_ += xml_escape(text.text);
        *out_ += "</text>";
    }

    void emit_image(const scene::ImageOp& image) override {
        *out_ += "<rect x=\"";
        *out_ += num(image.rect.x);
        *out_ += "\" y=\"";
        *out_ += num(image.rect.y);
        *out_ += "\" width=\"";
        *out_ += num(image.rect.w);
        *out_ += "\" height=\"";
        *out_ += num(image.rect.h);
        *out_ += "\" fill=\"none\" stroke=\"#888888\" data-image-id=\"";
        *out_ += std::to_string(image.image_id);
        *out_ += "\"/>";
    }

private:
    std::string* out_;
    const VectorOptions* opts_;
    const image::TtfFont* font_;  // 文字轮廓化字体；nullptr = 旧 <text> 行为
    std::string d_;  // 待刷出的折线子路径（由 MoveTo/LineTo 累积）
};

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

    const image::TtfFont* font = nullptr;
    std::shared_ptr<image::TtfFont> font_owned;
    if (opts_.text_outline && !opts_.outline_font.empty()) {
        if (auto r = image::TtfFont::load(opts_.outline_font)) {
            font_owned = std::make_shared<image::TtfFont>(std::move(*r));
            font = font_owned.get();
        }
    }

    SvgVisitor visitor{out_, opts_, font};
    visitor.run(scene);

    if (use_transform) {
        out_ += "</g>";
    }
    out_ += "</svg>";
}

}  // namespace termchart::backend::vector
