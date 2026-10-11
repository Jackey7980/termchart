#include "termchart/util/wcwidth.hpp"

#include "termchart/detail/unicode_tables.hpp"
#include "termchart/util/grapheme.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::util {

int codepoint_width(char32_t cp, const WidthOptions& options) noexcept {
    if (termchart::detail::is_zero_width(cp)) {
        return 0;
    }
    // emoji 策略**优先于**东亚宽：U+1F600 这类码点同时落在 East_Asian_Wide 与
    // Extended_Pictographic 里，若先判东亚宽，emoji_wide=false 就永远收不回 1 宽。
    if (is_extended_pictographic(cp)) {
        return options.emoji_wide ? 2 : 1;
    }
    if (termchart::detail::is_east_asian_wide(cp)) {
        return 2;
    }
    if (options.ambiguous_wide && termchart::detail::is_east_asian_ambiguous(cp)) {
        return 2;
    }
    return 1;
}

int cluster_width(const GraphemeCluster& cluster, const WidthOptions& options) noexcept {
    return codepoint_width(cluster.base(), options);
}

int display_width(std::string_view text, const WidthOptions& options) noexcept {
    int total = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t step = next_boundary(text, pos);
        if (step == 0) {
            break;
        }
        const Utf8Decode head = decode(text, pos);
        if (head.consumed != 0) {
            total += codepoint_width(head.cp, options);
        }
        pos += step;
    }
    return total;
}

std::size_t prefix_by_width(std::string_view text, int budget,
                            const WidthOptions& options) noexcept {
    if (budget <= 0) {
        return 0;
    }
    int used = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t step = next_boundary(text, pos);
        if (step == 0) {
            break;
        }
        const Utf8Decode head = decode(text, pos);
        const int width = head.consumed == 0 ? 0 : codepoint_width(head.cp, options);
        if (used + width > budget) {
            break;
        }
        used += width;
        pos += step;
    }
    return pos;  // 始终落在簇边界上
}

}  // namespace termchart::util
