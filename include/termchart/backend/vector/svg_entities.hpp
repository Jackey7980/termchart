// svg_entities.hpp —— SVG 序列化原语（转义 / 颜色 / 数字格式）
//
// header-only：纯函数，无状态。颜色复用 color/ 的调色板换算（from_index_16 /
// from_index_256），因此 SVG 与终端两条路径对同一 Color 的观感一致。
#ifndef TERMCHART_BACKEND_VECTOR_SVG_ENTITIES_HPP
#define TERMCHART_BACKEND_VECTOR_SVG_ENTITIES_HPP

#include <cstdio>
#include <string>
#include <string_view>

#include "termchart/color/quantize.hpp"
#include "termchart/core/style.hpp"

namespace termchart::backend::vector {

// XML 文本转义（& < > " '）。
inline std::string xml_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
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
            case '\'':
                out += "&apos;";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

// Rgb → "#RRGGBB"。
inline std::string rgb_hex(Rgb color) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", color.r, color.g, color.b);
    return buf;
}

// Color → SVG paint；None 表示"不画"（fill/stroke 用 "none"）。
inline std::string svg_color(const Color& color) {
    switch (color.kind()) {
        case ColorKind::None:
            return "none";
        case ColorKind::Ansi16:
            return rgb_hex(color::from_index_16(color.index()));
        case ColorKind::Indexed:
            return rgb_hex(color::from_index_256(color.index()));
        case ColorKind::Rgb:
            return rgb_hex(color.rgb());
    }
    return "none";
}

// 稳定数字格式：最多 3 位小数、去尾零（golden 需要确定性）。
inline std::string num(double value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.3f", value);
    std::string s(buf);
    if (s.find('.') != std::string::npos) {
        while (s.size() > 1 && s.back() == '0') {
            s.pop_back();
        }
        if (s.back() == '.') {
            s.pop_back();
        }
    }
    if (s == "-0") {
        s = "0";
    }
    return s;
}

}  // namespace termchart::backend::vector

#endif  // TERMCHART_BACKEND_VECTOR_SVG_ENTITIES_HPP
