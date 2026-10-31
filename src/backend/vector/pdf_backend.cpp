// pdf_backend.cpp —— Scene → PDF（极简 PDF 1.4 后端）
//
// 输出结构：
//   %PDF-1.4
//   1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj
//   2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj
//   3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 W H]
//                  /Resources << /Font << /F1 5 0 R >>
//                               /XObject << /Im0 6 0 R >> >>
//                  /Contents 4 0 R >> endobj
//   4 0 obj << /Length N >> stream ... endstream endobj
//   5 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> endobj
//   6 0 obj << /Type /XObject /Subtype /Image /Width ... /Height ...
//                  /ColorSpace /DeviceRGB /BitsPerComponent 8
//                  /Filter /FlateDecode /Length N >>
//             stream ... endstream endobj
//   xref\n0 7\n...\ntrailer << /Size 7 /Root 1 0 R >>\nstartxref\nN\n%%EOF
//
// 坐标系：PDF 原点在左下，y 向上；Scene 原点在左上，y 向下。
// 这里简单：直接用 PDF 原点（左下）→ y 上 = Scene y 反向（opts.height_pt - y）。
// 填充：rg/RG 设色，f/B/f* 发填充（不重画描边）；s/S 发描边（不重画填充）；
// re/rectangle 直接给一个矩形 + 填充或描边。
#include "termchart/backend/vector/pdf_backend.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "termchart/color/quantize.hpp"
#include "termchart/core/style.hpp"
#include "termchart/image/ttf_font.hpp"
#include "termchart/scene/stateful_visitor.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::backend::vector {

namespace {

std::string num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.3f", v);
    std::string s(buf);
    if (s.find('.') != std::string::npos) {
        while (s.size() > 1 && s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    return s;
}

void emit_color(std::string& out, const Color& c, const char* op) {
    if (c.is_none()) {
        out += op;
        out += " g\n";
        return;
    }
    Rgb rgb;
    switch (c.kind()) {
    case ColorKind::None:     rgb = {0, 0, 0}; break;
    case ColorKind::Ansi16:   rgb = color::from_index_16(c.index()); break;
    case ColorKind::Indexed:  rgb = color::from_index_256(c.index()); break;
    case ColorKind::Rgb:      rgb = c.rgb(); break;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.3f %.3f %.3f %s\n",
                  rgb.r / 255.0, rgb.g / 255.0, rgb.b / 255.0, op);
    out += buf;
}

class PdfVisitor final : public scene::StatefulVisitor {
public:
    PdfVisitor(std::string& content, double h, const image::TtfFont* font, double font_px) noexcept
        : content_(&content), height_pt_(h), font_(font), font_px_(font_px) {}

protected:
    void begin_path(const Point& from) override {
        append("M ", from);
    }
    void extend_path(const Point& from, const Point& to) override {
        (void)from;
        append("L ", to);
    }
    void end_path() override {
        if (path_.empty()) return;
        *content_ += path_;
        if (!current_fill().is_none()) *content_ += "B\n";
        else if (!current_stroke().is_none()) *content_ += "S\n";
        else *content_ += "n\n";
        path_.clear();
    }

    void emit_filled_rect(const Rect& rect, const Color& fill) override {
        *content_ += num(rect.x); *content_ += " ";
        *content_ += num(height_pt_ - rect.bottom()); *content_ += " ";
        *content_ += num(rect.w); *content_ += " ";
        *content_ += num(rect.h); *content_ += " re\n";
        emit_color(*content_, fill, "f");
    }
    void emit_stroked_rect(const Rect& rect, const Color& stroke, double width) override {
        *content_ += num(rect.x); *content_ += " ";
        *content_ += num(height_pt_ - rect.bottom()); *content_ += " ";
        *content_ += num(rect.w); *content_ += " ";
        *content_ += num(rect.h); *content_ += " re\n";
        char buf[32]; std::snprintf(buf, sizeof(buf), "%.3f w\n", width);
        *content_ += buf;
        emit_color(*content_, stroke, "S");
    }
    void emit_arc(const scene::ArcOp& arc, const Color& stroke, double width) override {
        // ArcOp：弧度、从 start 逆时针到 end（整圆 = {0, 2π}）。
        // PDF 无原生圆弧算子，用折线逼近；坐标翻转见文件头（y 上 = height_pt_ - y）。
        static constexpr double kStep = 0.12;  // ≈7°，足够平滑
        const int segs = std::max(2, static_cast<int>(
            std::ceil(std::fabs(arc.end - arc.start) / kStep)));
        std::string path;
        for (int i = 0; i <= segs; ++i) {
            const double t = arc.start + (arc.end - arc.start) *
                                (static_cast<double>(i) / segs);
            const double x = arc.center.x + arc.radius * std::cos(t);
            const double y = arc.center.y + arc.radius * std::sin(t);
            path += (i == 0 ? "M " : "L ");
            path += num(x); path += " ";
            path += num(height_pt_ - y); path += "\n";
        }
        *content_ += path;
        char buf[32]; std::snprintf(buf, sizeof(buf), "%.3f w\n", width);
        *content_ += buf;
        emit_color(*content_, stroke, "S");
    }
    void emit_text(const scene::TextOp& text) override {
        // 文字轮廓化：把每个字形转成自包含矢量路径（不再依赖 Helvetica）。
        if (font_ != nullptr) {
            const double baseline = text.at.y + font_->metrics(font_px_).ascent;
            const Color fill = text.style.fg();
            double pen_x = text.at.x;
            std::size_t pos = 0;
            while (pos < text.text.size()) {
                const auto dec = util::decode(text.text, pos);
                pos = static_cast<std::size_t>(dec.consumed);
                const auto go = font_->glyph_outline(dec.cp, font_px_);
                if (go && !go->contours.empty()) {
                    for (const auto& contour : go->contours) {
                        *content_ += "M ";
                        for (std::size_t i = 0; i < contour.size(); ++i) {
                            const auto& p = contour[i];
                            if (i) *content_ += ' ';
                            *content_ += num(pen_x + p.x);
                            *content_ += ' ';
                            *content_ += num(height_pt_ - (baseline + p.y));
                        }
                        *content_ += " h\n";
                    }
                    emit_color(*content_, fill, "rg");  // 设置填充色
                    *content_ += "f\n";                  // 填充闭合路径
                }
                pen_x += go ? go->advance_x : (font_px_ * 0.5);
            }
            return;
        }
        // 旧行为：Helvetica Tj
        *content_ += "BT\n/F1 12 Tf\n";
        *content_ += num(text.at.x); *content_ += " ";
        *content_ += num(height_pt_ - text.at.y); *content_ += " Td\n";
        std::string esc;
        esc.reserve(text.text.size());
        for (char c : text.text) {
            if (c == '(' || c == ')' || c == '\\') esc += '\\';
            esc += c;
        }
        *content_ += "(";
        *content_ += esc;
        *content_ += ") Tj\nET\n";
    }
    void emit_image(const scene::ImageOp& image) override {
        // 占位：emit 1×1 白 XObject（位图嵌入未实现，见 hpp 头注释）
        (void)image;
        *content_ += "0 0 0 rg\n0 0 0 RG\n";
        *content_ += "q\n";
        *content_ += num(image.rect.x); *content_ += " 0 0 ";
        *content_ += num(image.rect.w); *content_ += " ";
        *content_ += num(image.rect.h); *content_ += " cm\n";
        *content_ += "/Im0 Do\nQ\n";
    }

private:
    void append(const char* op, const Point& p) {
        *content_ += op;
        *content_ += num(p.x);
        *content_ += " ";
        *content_ += num(height_pt_ - p.y);
        *content_ += " ";
        path_ += op;
        path_ += num(p.x);
        path_ += " ";
        path_ += num(height_pt_ - p.y);
        path_ += " ";
    }

    std::string* content_;
    double        height_pt_;
    const image::TtfFont* font_;
    double        font_px_;
    std::string   path_;
};

}  // namespace

void PdfBackend::end() {
    const std::string header = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    std::ostringstream oss;
    oss << header;

    // 简化版：写所有对象在内存里，结尾再写 xref。
    std::vector<std::string> objs;
    std::vector<std::size_t> offsets;
    auto push = [&](std::string s) {
        offsets.push_back(static_cast<std::size_t>(oss.tellp()));
        oss << s;
        objs.push_back(std::move(s));
    };

    // Catalog
    push("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    // Pages
    push("2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
    // Page
    {
        std::ostringstream p;
        p << "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 "
          << num(opts_.width_pt) << " " << num(opts_.height_pt)
          << "] /Resources << /Font << /F1 5 0 R >> "
          << "/XObject << /Im0 6 0 R >> >> /Contents 4 0 R >>\nendobj\n";
        push(p.str());
    }
    // Contents stream
    {
        std::ostringstream c;
        c << "4 0 obj\n<< /Length " << out_.size() << " >>\nstream\n";
        c << out_;
        c << "\nendstream\nendobj\n";
        push(c.str());
    }
    // Font
    push("5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\nendobj\n");
    // Image XObject (placeholder)
    push("6 0 obj\n<< /Type /XObject /Subtype /Image /Width 1 /Height 1 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Length 3 >>\nstream\n\xFF\xFF\xFF\nendstream\nendobj\n");

    const auto xref_pos = static_cast<std::size_t>(oss.tellp());
    oss << "xref\n0 " << (offsets.size() + 1) << "\n";
    oss << "0000000000 65535 f \n";
    for (auto off : offsets) {
        char buf[32]; std::snprintf(buf, sizeof(buf), "%010zu 00000 n \n", off);
        oss << buf;
    }
    oss << "trailer\n<< /Size " << (offsets.size() + 1) << " /Root 1 0 R >>\n";
    oss << "startxref\n" << xref_pos << "\n%%EOF\n";
    out_ = oss.str();
}

void PdfBackend::render(const scene::Scene& scene) {
    const image::TtfFont* font = nullptr;
    if (opts_.text_outline && !opts_.outline_font.empty()) {
        if (auto r = image::TtfFont::load(opts_.outline_font)) {
            outline_font_ = std::make_shared<image::TtfFont>(std::move(*r));
            font = outline_font_.get();
        }
    }
    PdfVisitor visitor(out_, opts_.height_pt, font, 12.0);
    visitor.run(scene);
}

std::string render_pdf(PdfBackend& backend, const scene::Scene& scene) {
    backend.begin();
    backend.render(scene);
    backend.end();
    return backend.str();
}

}  // namespace termchart::backend::vector
