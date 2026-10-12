#include "termchart/chart/hierarchical/node.hpp"

#include <cstdint>

#include "termchart/util/utf8.hpp"

namespace termchart::chart::hier {

double aggregate_values(HierNode& node) {
    if (node.is_leaf()) {
        return node.value > 0.0 ? node.value : 0.0;
    }
    double sum = 0.0;
    for (HierNode& child : node.children) {
        sum += aggregate_values(child);
    }
    if (node.value <= 0.0) {
        node.value = sum;
    }
    return node.value;
}

Color hier_palette(std::size_t branch) noexcept {
    // 4 蓝 2 绿 6 青 3 黄 5 品红 1 红 7 灰白：循环但不取 0（黑）。
    static constexpr std::uint8_t kPalette[7] = {4, 2, 6, 3, 5, 1, 7};
    return Color::ansi16(kPalette[branch % 7]);
}

void assign_colors(HierNode& node, std::size_t branch, std::size_t depth) {
    if (depth == 0 && node.color.is_none()) {
        node.color = hier_palette(0);
    }
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        HierNode& child = node.children[i];
        const std::size_t child_branch = (depth == 0) ? i : branch;
        if (child.color.is_none()) {
            child.color = hier_palette(child_branch);
        }
        assign_colors(child, child_branch, depth + 1);
    }
}

char32_t first_codepoint(const std::string& text) {
    if (text.empty()) {
        return U'\u0020';
    }
    return util::decode(text, 0).cp;
}

}  // namespace termchart::chart::hier
