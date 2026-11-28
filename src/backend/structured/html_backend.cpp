// backend/structured/html_backend.cpp —— Document → HTML 实现
#include "termchart/backend/structured/html_backend.hpp"

#include <cstdio>
#include <string>

#include "chart_svg.hpp"
#include "termchart/color/quantize.hpp"
#include "termchart/document/block.hpp"
#include "termchart/document/span.hpp"
#include "termchart/document/table_model.hpp"

namespace termchart::backend::structured {

namespace {

void append_escaped(std::string& out, std::string_view text) {
    for (char c : text) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            default:
                out += c;
                break;
        }
    }
}

// 颜色 → CSS hex；None 返回空串（表示"不额外包 span"）。
std::string css_color(const Color& color) {
    Rgb rgb{};
    switch (color.kind()) {
        case ColorKind::None:
            return {};
        case ColorKind::Ansi16:
            rgb = color::from_index_16(color.index());
            break;
        case ColorKind::Indexed:
            rgb = color::from_index_256(color.index());
            break;
        case ColorKind::Rgb:
            rgb = color.rgb();
            break;
    }
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", rgb.r, rgb.g, rgb.b);
    return buf;
}

}  // namespace

void HtmlBackend::render(const document::Document& doc) {
    out_.clear();
    const std::string title =
        opts_.title.empty() ? std::string("termchart") : opts_.title;
    out_ += "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<title>";
    append_escaped(out_, title);
    out_ += "</title>\n</head>\n<body class=\"";
    append_escaped(out_, opts_.css_class);
    out_ += "\">\n";

    for (const document::Block& b : doc.blocks()) {
        switch (b.kind()) {
            case document::BlockKind::Heading: {
                int level = b.level();
                level = level < 1 ? 1 : (level > 6 ? 6 : level);
                const std::string tag = std::to_string(level);
                out_ += "<h" + tag + ">";
                append_escaped(out_, b.text());
                out_ += "</h" + tag + ">\n";
                break;
            }
            case document::BlockKind::Paragraph: {
                out_ += "<p>";
                for (const document::Span& s : b.spans()) {
                    const std::string color = css_color(s.style.fg());
                    if (color.empty()) {
                        append_escaped(out_, s.text);
                    } else {
                        out_ += "<span style=\"color:";
                        out_ += color;
                        out_ += "\">";
                        append_escaped(out_, s.text);
                        out_ += "</span>";
                    }
                }
                out_ += "</p>\n";
                break;
            }
            case document::BlockKind::Code: {
                out_ += "<pre><code";
                if (!b.language().empty()) {
                    out_ += " class=\"language-";
                    append_escaped(out_, b.language());
                    out_ += "\"";
                }
                out_ += ">";
                append_escaped(out_, b.text());
                out_ += "</code></pre>\n";
                break;
            }
            case document::BlockKind::Table: {
                const document::TableModel& t = b.table();
                out_ += "<table>\n";
                if (!t.header().empty()) {
                    out_ += "<thead><tr>";
                    for (const std::string& h : t.header()) {
                        out_ += "<th>";
                        append_escaped(out_, h);
                        out_ += "</th>";
                    }
                    out_ += "</tr></thead>\n";
                }
                out_ += "<tbody>\n";
                for (const auto& row : t.rows()) {
                    out_ += "<tr>";
                    for (const std::string& c : row) {
                        out_ += "<td>";
                        append_escaped(out_, c);
                        out_ += "</td>";
                    }
                    out_ += "</tr>\n";
                }
                out_ += "</tbody>\n</table>\n";
                break;
            }
            case document::BlockKind::Chart:
                out_ += "<div class=\"termchart-chart\">";
                out_ += detail::chart_svg_or_placeholder(b.scene());
                out_ += "</div>\n";
                break;
        }
    }
    out_ += "</body>\n</html>\n";
}

}  // namespace termchart::backend::structured
