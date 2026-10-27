// eps_backend.cpp —— Scene → EPS（PostScript 子集，EPSF-1.2 后端）
//
// 输出结构（无 xref / 无对象表，纯 PostScript 内容流）：
//   %!PS-Adobe-2.0 EPSF-1.2
//   %%BoundingBox: 0 0 W H
//   %%Pages: 1
//   %%EndComments
//   /F1 /Helvetica findfont 12 scalefont def
//   <content stream: M/L/re/rg/RG/f/S/B/w/BT/Tf/Td/Tj/ET ...>
//   showpage        （showpage=false 时省略，严格 EPSF）
//   %%EOF
//
// 坐标系：PostScript 原点在左下，y 向上；Scene 原点在左上，y 向下。
// 这里用 PostScript 原点 → y 上 = height_pt_ - y，与 pdf_backend 同构。
// 设色语义（关键，与 pdf_backend 的潜在 bug 不同，这里写对）：先 rg/RG 设填充/描边色，
// 再 f/S/B 触发填充/描边——颜色算子与填充命令分开，不能把 "f" 当颜色占位符。
#include "termchart/backend/vector/eps_backend.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "termchart/color/quantize.hpp"
#include "termchart/core/style.hpp"
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

// 设填充/描边色：op 必须是 "rg"（填充）或 "RG"（描边）。颜色与命令必须分两条发。
void emit_color(std::string& out, const Color& c, const char* op) {
    Rgb rgb{0, 0, 0};
    switch (c.kind()) {
    case ColorKind::None:    rgb = {0, 0, 0}; break;
    case ColorKind::Ansi16:  rgb = color::from_index_16(c.index()); break;
    case ColorKind::Indexed: rgb = color::from_index_256(c.index()); break;
    case ColorKind::Rgb:     rgb = c.rgb(); break;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.3f %.3f %.3f %s\n",
                  rgb.r / 255.0, rgb.g / 255.0, rgb.b / 255.0, op);
    out += buf;
}

class EpsVisitor final : public scene::StatefulVisitor {
public:
    EpsVisitor(std::string& content, double h) noexcept
        : content_(&content), height_pt_(h) {}

protected:
    void begin_path(const Point& from) override {
        path_ = "M " + num(from.x) + " " + num(height_pt_ - from.y) + " ";
    }
    void extend_path(const Point& from, const Point& to) override {
        (void)from;
        path_ += "L " + num(to.x) + " " + num(height_pt_ - to.y) + " ";
    }
    void end_path() override {
        if (path_.empty()) return;
        *content_ += path_;
        if (!current_fill().is_none() && !current_stroke().is_none()) {
            emit_color(*content_, current_fill(), "rg");
            emit_color(*content_, current_stroke(), "RG");
            *content_ += "B\n";   // 填充 + 描边（用各自已设色）
        } else if (!current_fill().is_none()) {
            emit_color(*content_, current_fill(), "rg");
            *content_ += "f\n";   // 仅填充
        } else if (!current_stroke().is_none()) {
            emit_color(*content_, current_stroke(), "RG");
            *content_ += "S\n";   // 仅描边
        } else {
            *content_ += "n\n";   // 仅建路径不画
        }
        path_.clear();
    }

    void emit_filled_rect(const Rect& rect, const Color& fill) override {
        *content_ += num(rect.x); *content_ += " ";
        *content_ += num(height_pt_ - rect.bottom()); *content_ += " ";
        *content_ += num(rect.w); *content_ += " ";
        *content_ += num(rect.h); *content_ += " re\n";
        emit_color(*content_, fill, "rg");
        *content_ += "f\n";
    }
    void emit_stroked_rect(const Rect& rect, const Color& stroke, double width) override {
        *content_ += num(rect.x); *content_ += " ";
        *content_ += num(height_pt_ - rect.bottom()); *content_ += " ";
        *content_ += num(rect.w); *content_ += " ";
        *content_ += num(rect.h); *content_ += " re\n";
        char buf[32]; std::snprintf(buf, sizeof(buf), "%.3f w\n", width);
        *content_ += buf;
        emit_color(*content_, stroke, "RG");
        *content_ += "S\n";
    }
    void emit_arc(const scene::ArcOp& arc, const Color& stroke, double width) override {
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
        emit_color(*content_, stroke, "RG");
        *content_ += "S\n";
    }
    void emit_text(const scene::TextOp& text) override {
        // EPSF 不嵌入字体：用标准 Helvetica（ASCII only）。
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
        // 占位：白矩形（位图嵌入未实现，见 hpp 头注释）。
        *content_ += num(image.rect.x); *content_ += " ";
        *content_ += num(height_pt_ - image.rect.bottom()); *content_ += " ";
        *content_ += num(image.rect.w); *content_ += " ";
        *content_ += num(image.rect.h); *content_ += " re\n";
        *content_ += "1 1 1 rg\nf\n";
    }

private:
    std::string* content_;
    double       height_pt_;
    std::string  path_;
};

}  // namespace

void EpsBackend::end() {
    std::ostringstream oss;
    oss << "%!PS-Adobe-2.0 EPSF-1.2\n";
    oss << "%%BoundingBox: 0 0 " << num(opts_.width_pt) << " "
        << num(opts_.height_pt) << "\n";
    oss << "%%Pages: 1\n";
    oss << "%%EndComments\n";
    oss << "/F1 /Helvetica findfont 12 scalefont def\n";
    oss << out_;  // 内容流
    if (opts_.showpage) oss << "showpage\n";
    oss << "%%EOF\n";
    out_ = oss.str();
}

void EpsBackend::render(const scene::Scene& scene) {
    EpsVisitor visitor(out_, opts_.height_pt);
    visitor.run(scene);
}

std::string render_eps(EpsBackend& backend, const scene::Scene& scene) {
    backend.begin();
    backend.render(scene);
    backend.end();
    return backend.str();
}

}  // namespace termchart::backend::vector
