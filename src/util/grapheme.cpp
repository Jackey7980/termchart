#include "termchart/util/grapheme.hpp"

#include "termchart/detail/grapheme_tables.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::util {
namespace {

using termchart::detail::Gb;

Gb class_of(char32_t cp) noexcept { return termchart::detail::grapheme_class(static_cast<std::uint32_t>(cp)); }

// 相邻两个码点之间是否**不允许**断开（GB6..GB9b / GB11..GB13）。
bool should_keep_together(Gb prev, std::size_t prev_run, Gb next) noexcept {
    // GB4 / GB5：控制符（含 CR / LF）自成簇。**必须排在 GB9 之前**——
    // 否则 "\n" + 组合记号会被误并成一簇。
    if (next == Gb::Control || next == Gb::CR || next == Gb::LF) {
        return false;
    }
    if (prev == Gb::Control || prev == Gb::CR || prev == Gb::LF) {
        return false;
    }
    // GB9b：Prepend × 任意（Prepend 向前吸收，所以只看 prev）。
    if (prev == Gb::Prepend) {
        return true;
    }
    if (next == Gb::Any) {
        return false;  // GB999：两个普通字符之间断开
    }
    // GB9 / GB9a：**任意**基字符 × (Extend | ZWJ | SpacingMark)。
    // 注意这里不能要求 prev != Any —— 绝大多数基字符（'e'、'中'）的 GCB 就是 Any。
    if (next == Gb::Extend || next == Gb::ZWJ || next == Gb::SpacingMark) {
        return true;
    }
    // GB6..GB8：Hangul 组合。
    if (prev == Gb::L && (next == Gb::L || next == Gb::V || next == Gb::LV || next == Gb::LVT)) {
        return true;
    }
    if ((prev == Gb::LV || prev == Gb::V) && (next == Gb::V || next == Gb::T)) {
        return true;
    }
    if ((prev == Gb::LVT || prev == Gb::T) && next == Gb::T) {
        return true;
    }
    // GB12/GB13：地区旗标成对出现，奇数个不断开。
    if (prev == Gb::RegionalIndicator && next == Gb::RegionalIndicator) {
        return (prev_run % 2) == 1;
    }
    return false;
}

}  // namespace

bool is_extended_pictographic(char32_t cp) noexcept {
    const std::uint32_t value = static_cast<std::uint32_t>(cp);
    // 覆盖主要 emoji 区段的近似判定；完整集合见 UAX #51，阶段 7 补齐。
    return (value >= 0x1F000u && value <= 0x1FAFFu) || (value >= 0x2600u && value <= 0x27BFu) ||
           value == 0x00A9u || value == 0x00AEu || value == 0x203Cu || value == 0x2049u ||
           (value >= 0x2122u && value <= 0x2139u) || (value >= 0x2194u && value <= 0x21AAu) ||
           (value >= 0x231Au && value <= 0x23FAu) || (value >= 0x24C2u && value <= 0x24C2u) ||
           (value >= 0x25A0u && value <= 0x25FFu) || (value >= 0x2B00u && value <= 0x2BFFu) ||
           (value >= 0xFE0Fu && value <= 0xFE0Fu);
}

std::size_t next_boundary(std::string_view text, std::size_t pos) noexcept {
    if (pos >= text.size()) {
        return 0;
    }
    const Utf8Decode first = decode(text, pos);
    if (first.consumed == 0) {
        return 0;
    }
    std::size_t cursor = pos + static_cast<std::size_t>(first.consumed);
    Gb prev = first.ok ? class_of(first.cp) : Gb::Any;
    std::size_t prev_run = 1;
    Gb prev_raw = prev;

    while (cursor < text.size()) {
        const Utf8Decode d = decode(text, cursor);
        if (d.consumed == 0) {
            break;
        }
        const Gb next = d.ok ? class_of(d.cp) : Gb::Any;

        // GB3：CR × LF
        const bool crlf = prev_raw == Gb::CR && next == Gb::LF;
        // GB11：ZWJ 后紧跟扩展象形 → 不断开
        const bool zwj_pair = prev_raw == Gb::ZWJ && d.ok && is_extended_pictographic(d.cp);

        if (!crlf && !zwj_pair && !should_keep_together(prev, prev_run, next)) {
            break;
        }
        // GB4/GB5：控制符自成簇，已由"非 Extension 即断开"覆盖。
        if (next == prev_raw) {
            ++prev_run;
        } else {
            prev_run = 1;
        }
        prev_raw = next;
        prev = next;
        cursor += static_cast<std::size_t>(d.consumed);
    }
    return cursor - pos;
}

std::size_t cluster_count(std::string_view text) noexcept {
    std::size_t count = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t step = next_boundary(text, pos);
        if (step == 0) {
            break;
        }
        pos += step;
        ++count;
    }
    return count;
}

std::vector<GraphemeCluster> split_clusters(std::string_view text) {
    std::vector<GraphemeCluster> clusters;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t step = next_boundary(text, pos);
        if (step == 0) {
            break;
        }
        const std::string_view piece = text.substr(pos, step);
        std::size_t inner = 0;
        const Utf8Decode head = decode(piece, inner);
        if (head.consumed == 0) {
            break;
        }
        GraphemeCluster cluster(head.cp);
        inner += static_cast<std::size_t>(head.consumed);
        while (inner < piece.size()) {
            const Utf8Decode d = decode(piece, inner);
            if (d.consumed == 0) {
                break;
            }
            cluster.add_side_bearing(d.cp);
            inner += static_cast<std::size_t>(d.consumed);
        }
        clusters.push_back(cluster);
        pos += step;
    }
    return clusters;
}

}  // namespace termchart::util
