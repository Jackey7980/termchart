// syntax_tree.cpp —— 语法树实现
#include "termchart/graph/diagrams/syntax_tree.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::graph {

Rect render_syntax_tree(const SyntaxNode& root, canvas::Canvas& canvas,
                        const SyntaxTreeOptions& opts) {
    const int indent = std::max(3, opts.indent);
    Rect bounds{};
    std::uint32_t row = 0;

    // 显式栈：node + 已发出的子节点数 + 该节点是否为父节点的末子。
    // last_flags[l] 与 stack[l] 对齐：祖先是否"末子"决定其连接列下方画不画 │。
    struct Frame {
        const SyntaxNode* node;
        std::size_t next;
        bool last;
    };
    std::vector<Frame> stack;

    // 发出一个节点（depth = 发出时栈深）。祖先延续线看栈里 1..depth-1 的帧。
    auto emit = [&](const std::string& label, std::size_t depth, bool last) {
        const double y = static_cast<double>(row);
        const double col =
            static_cast<double>(depth) * static_cast<double>(indent);
        for (std::size_t l = 1; l < depth; ++l) {
            if (!stack[l].last) {
                canvas.put(Point{static_cast<double>(l) * static_cast<double>(indent), y},
                           U'\u2502', opts.line_style);
            }
        }
        if (depth > 0) {
            canvas.put(Point{col, y}, last ? U'\u2514' : U'\u251C', opts.line_style);
            for (int k = 1; k <= indent - 2; ++k) {
                canvas.put(Point{col + static_cast<double>(k), y}, U'\u2500',
                           opts.line_style);
            }
        }
        const double label_col = depth > 0 ? col + static_cast<double>(indent - 1) : 0.0;
        if (!label.empty()) {
            canvas.draw_text(Point{label_col, y}, label, opts.label_style);
        }
        bounds = bounds.union_with(Rect{label_col, y,
                                        static_cast<double>(util::display_width(label)),
                                        1.0});
        ++row;
    };

    emit(root.label, 0, true);
    if (!root.children.empty()) {
        stack.push_back(Frame{&root, 0, true});
    }
    while (!stack.empty()) {
        Frame& top = stack.back();
        if (top.next < top.node->children.size()) {
            const SyntaxNode& child = top.node->children[top.next];
            const bool last = (top.next + 1 == top.node->children.size());
            ++top.next;
            emit(child.label, stack.size(), last);
            stack.push_back(Frame{&child, 0, last});
        } else {
            stack.pop_back();
        }
    }
    return bounds;
}

}  // namespace termchart::graph
