#include "termchart/util/text.hpp"

#include "termchart/util/grapheme.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::util {

std::string repeat(char32_t cp, std::size_t n) {
    std::string out;
    char buffer[kMaxUtf8Bytes]{};
    const std::size_t unit = encode(cp, buffer);
    if (unit == 0) {
        return out;
    }
    out.reserve(unit * n);
    for (std::size_t i = 0; i < n; ++i) {
        out.append(buffer, unit);
    }
    return out;
}

std::string pad(std::string_view text, int width, Align align, char32_t pad_char) {
    const int current = display_width(text);
    if (current >= width) {
        return std::string(text);
    }
    const int deficit = width - current;
    switch (align) {
        case Align::Right:
            return repeat(pad_char, static_cast<std::size_t>(deficit)) + std::string(text);
        case Align::Center: {
            const int left = deficit / 2;
            const int right = deficit - left;
            return repeat(pad_char, static_cast<std::size_t>(left)) + std::string(text) +
                   repeat(pad_char, static_cast<std::size_t>(right));
        }
        case Align::Left:
        default:
            break;
    }
    return std::string(text) + repeat(pad_char, static_cast<std::size_t>(deficit));
}

std::string truncate(std::string_view text, int max_width, Align align,
                     std::string_view ellipsis) {
    const int width = display_width(text);
    if (width <= max_width) {
        return std::string(text);
    }
    const int ellipsis_width = display_width(ellipsis);
    if (max_width <= ellipsis_width) {
        // 连省略符都放不下：硬切到可见部分。
        return std::string(text.substr(0, prefix_by_width(text, max_width)));
    }
    const std::size_t keep = prefix_by_width(text, max_width - ellipsis_width);
    const std::string body(text.substr(0, keep));
    return pad(body + std::string(ellipsis), max_width, align);
}

std::vector<std::string> wrap(std::string_view text, int width, bool force_break) {
    std::vector<std::string> lines;
    if (width <= 0) {
        return lines;
    }
    std::size_t pos = 0;
    std::string current;
    int current_width = 0;
    while (pos < text.size()) {
        const std::size_t step = next_boundary(text, pos);
        if (step == 0) {
            break;
        }
        const std::string_view piece = text.substr(pos, step);
        const int piece_width = display_width(piece);
        if (current_width + piece_width > width) {
            if (!current.empty()) {
                lines.push_back(current);
                current.clear();
                current_width = 0;
            }
            if (piece_width > width && force_break) {
                // 单个不可再分的簇本身就比整行还宽：整簇保留（宁可横向溢出一行，也绝不
                // 丢字）。绝不能按 prefix_by_width 取前缀再跳过整个 piece —— prefix_by_width
                // 对"超宽首簇"返回 0，那种写法等于把整簇静默吞掉（见雷区2）。
                current = std::string(piece);
                lines.push_back(current);
                current.clear();
                current_width = 0;
                pos += step;
                continue;
            }
        }
        current.append(piece);
        current_width += piece_width;
        pos += step;
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

std::string fit(std::string_view text, int max_width, Align align) {
    return pad(truncate(text, max_width, align), max_width, align);
}

}  // namespace termchart::util
