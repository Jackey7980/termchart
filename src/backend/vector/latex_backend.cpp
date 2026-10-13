// backend/vector/latex_backend.cpp —— CellBuffer → TikZ 实现
//
// 逐行扫描：─ 跑成 \draw、█ 跑成 \fill（两者按横向连续段合并，一格一段
// 会产生海量重复命令）、其余非空格逐格出 \node。同格内不再细分字形——
// TikZ 侧要的是可缩放的矢量版式，不是逐笔画复刻。
#include "termchart/backend/vector/latex_backend.hpp"

#include <cstdio>
#include <string>

#include "termchart/color/resolve.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::backend::vector {

namespace {

constexpr char32_t kBoxHorizontal = U'\u2500';  // ─
constexpr char32_t kFullBlock = U'\u2588';      // █

// 1 格 → 多少 pt，写进 tikzpicture 的 x/y 选项；%.10g 对 12 这类整值输出
// "12"，不产生尾零噪声。
std::string num_pt(double value) {
    char buf[32];
    (void)std::snprintf(buf, sizeof(buf), "%.10g", value);
    return buf;
}

// TikZ 节点文本的保留字符转义（最小集合：单字符单元里会遇到的那几个）。
void append_escaped(std::string& out, char32_t cp) {
    switch (cp) {
        case U'\\': out += "\\textbackslash{}"; return;
        case U'&': out += "\\&"; return;
        case U'%': out += "\\%"; return;
        case U'$': out += "\\$"; return;
        case U'#': out += "\\#"; return;
        case U'_': out += "\\_"; return;
        case U'{': out += "\\{"; return;
        case U'}': out += "\\}"; return;
        case U'~': out += "\\textasciitilde{}"; return;
        case U'^': out += "\\textasciicircum{}"; return;
        default: break;
    }
    util::append(out, cp);
}

// xcolor 内联色模型：{rgb,255:red,R;green,G;blue,B}。
// None 色在调用侧就不发键，走不到这里。
std::string xcolor_of(const Color& color) {
    const Rgb rgb = color::resolve_color(color);
    std::string out = "{rgb,255:red,";
    out += std::to_string(static_cast<unsigned>(rgb.r));
    out += ";green,";
    out += std::to_string(static_cast<unsigned>(rgb.g));
    out += ";blue,";
    out += std::to_string(static_cast<unsigned>(rgb.b));
    out += '}';
    return out;
}

}  // namespace

std::uint32_t LatexBackend::row_width(const CellBuffer& buffer,
                                      std::uint32_t y) const noexcept {
    std::uint32_t w = buffer.width();
    if (!opts_.trim_trailing_blank) {
        return w;
    }
    while (w > 0 && is_blank(buffer.at(w - 1, y))) {
        --w;
    }
    return w;
}

void LatexBackend::render(const CellBuffer& buffer) {
    out_.clear();
    if (opts_.standalone) {
        out_ += "\\documentclass{standalone}\n";
        out_ += "\\usepackage{tikz}\n";
        out_ += "\\usepackage{xcolor}\n";
        out_ += "\\begin{document}\n";
    }
    out_ += "\\begin{tikzpicture}[x=";
    out_ += num_pt(opts_.scale);
    out_ += "pt,y=";
    out_ += num_pt(opts_.scale);
    out_ += "pt]\n";

    for (std::uint32_t y = 0; y < buffer.height(); ++y) {
        const std::uint32_t w = row_width(buffer, y);
        for (std::uint32_t x = 0; x < w;) {
            const Cell& cell = buffer.at(x, y);
            if (is_continuation(cell) || cell.cp == U' ') {
                ++x;
                continue;
            }

            // TikZ 坐标向上为正：行号取负，让第一行落在最上方。
            const std::string at = " (" + std::to_string(x) + ",-" + std::to_string(y) + ")";
            if (cell.cp == kBoxHorizontal) {
                std::uint32_t x1 = x;
                while (x1 + 1 < w && buffer.at(x1 + 1, y).cp == kBoxHorizontal) {
                    ++x1;
                }
                out_ += "\\draw";
                if (!cell.style.fg().is_none()) {
                    out_ += "[draw=";
                    out_ += xcolor_of(cell.style.fg());
                    out_ += ']';
                }
                out_ += at;
                out_ += " -- (" + std::to_string(x1) + ",-" + std::to_string(y) + ");\n";
                x = x1 + 1;
                continue;
            }
            if (cell.cp == kFullBlock) {
                std::uint32_t x1 = x;
                while (x1 + 1 < w && buffer.at(x1 + 1, y).cp == kFullBlock) {
                    ++x1;
                }
                out_ += "\\fill";
                if (!cell.style.fg().is_none()) {
                    out_ += '[';
                    out_ += xcolor_of(cell.style.fg());
                    out_ += ']';
                }
                // 左右各扩半格：相邻段无缝衔接，单格也不是零面积。
                out_ += " (" + std::to_string(x) + ".5,-" + std::to_string(y) + ".5) rectangle (";
                out_ += std::to_string(x1 + 1);
                out_ += ".5,-" + std::to_string(y) + ".5);\n";
                x = x1 + 1;
                continue;
            }

            out_ += "\\node";
            if (!cell.style.fg().is_none()) {
                out_ += "[text=";
                out_ += xcolor_of(cell.style.fg());
                out_ += ']';
            }
            out_ += " at";
            out_ += at;
            out_ += " {";
            append_escaped(out_, cell.cp);
            out_ += "};\n";
            ++x;
        }
    }

    out_ += "\\end{tikzpicture}\n";
    if (opts_.standalone) {
        out_ += "\\end{document}\n";
    }
}

std::string render_latex(LatexBackend& backend, const CellBuffer& buffer) {
    backend.begin();
    backend.render(buffer);
    backend.end();
    return backend.str();
}

}  // namespace termchart::backend::vector
