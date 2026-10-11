// backend/snapshot/json_backend.cpp —— 结构化 JSON 导出
#include "termchart/backend/snapshot/json_backend.hpp"

#include <cstdint>

#include "termchart/core/cell.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::backend::snapshot {

namespace {

void append_json_string(std::string& out, const std::string& utf8) {
    out += '"';
    for (char raw : utf8) {
        const auto ch = static_cast<unsigned char>(raw);
        switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (ch < 0x20) {
                    static const char* kHex = "0123456789abcdef";
                    out += "\\u00";
                    out += kHex[(ch >> 4) & 0x0F];
                    out += kHex[ch & 0x0F];
                } else {
                    out += static_cast<char>(ch);
                }
                break;
        }
    }
    out += '"';
}

void append_color(std::string& out, const Color& color) {
    switch (color.kind()) {
        case ColorKind::None:
            out += "{\"kind\":\"none\"}";
            break;
        case ColorKind::Ansi16:
            out += "{\"kind\":\"ansi16\",\"index\":";
            out += std::to_string(color.index());
            out += '}';
            break;
        case ColorKind::Indexed:
            out += "{\"kind\":\"indexed\",\"index\":";
            out += std::to_string(color.index());
            out += '}';
            break;
        case ColorKind::Rgb: {
            const Rgb rgb = color.rgb();
            out += "{\"kind\":\"rgb\",\"r\":";
            out += std::to_string(rgb.r);
            out += ",\"g\":";
            out += std::to_string(rgb.g);
            out += ",\"b\":";
            out += std::to_string(rgb.b);
            out += '}';
            break;
        }
    }
}

}  // namespace

void JsonBackend::render(const CellBuffer& buffer) {
    out_.clear();
    out_ += "{\"width\":";
    out_ += std::to_string(buffer.width());
    out_ += ",\"height\":";
    out_ += std::to_string(buffer.height());
    out_ += ",\"rows\":[";
    for (std::uint32_t y = 0; y < buffer.height(); ++y) {
        if (y != 0) {
            out_ += ',';
        }
        out_ += '[';
        for (std::uint32_t x = 0; x < buffer.width(); ++x) {
            if (x != 0) {
                out_ += ',';
            }
            const Cell& cell = buffer.at(x, y);
            if (is_continuation(cell)) {
                out_ += "null";
                continue;
            }
            out_ += "{\"cp\":";
            out_ += std::to_string(static_cast<std::uint32_t>(cell.cp));
            out_ += ",\"ch\":";
            append_json_string(out_, util::to_utf8(cell.cp));
            out_ += ",\"fg\":";
            append_color(out_, cell.style.fg());
            out_ += ",\"bg\":";
            append_color(out_, cell.style.bg());
            out_ += ",\"attrs\":";
            out_ += std::to_string(static_cast<std::uint32_t>(cell.style.attrs()));
            out_ += '}';
        }
        out_ += ']';
    }
    out_ += "]}";
}

}  // namespace termchart::backend::snapshot
