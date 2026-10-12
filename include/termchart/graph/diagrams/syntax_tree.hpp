// syntax_tree.hpp —— 语法树（(label, children) 递归节点 + tree 风格连接线）
//
// 字符化策略：根在左、深度沿列向右增长，逐层用 ├ └ ─ 画连接、祖先未完的层画
// │ 延续——即终端 tree 命令的形态；一行一个节点，超宽标签由画布裁剪。遍历用
// 显式栈（深树不会爆调用栈）。
#ifndef TERMCHART_GRAPH_DIAGRAMS_SYNTAX_TREE_HPP
#define TERMCHART_GRAPH_DIAGRAMS_SYNTAX_TREE_HPP

#include <string>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct SyntaxNode {
    std::string label;
    std::vector<SyntaxNode> children;
};

struct SyntaxTreeOptions {
    int indent = 4;  // 每层缩进格数（<3 时按 3 处理，否则放不下 ├─ 与间隔）
    Style label_style{};
    Style line_style{};
};

// 就地绘制；返回整棵树的包围盒（空标签的根返回零矩形）。
[[nodiscard]] TERMCHART_API Rect render_syntax_tree(const SyntaxNode& root,
                                                    canvas::Canvas& canvas,
                                                    const SyntaxTreeOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_SYNTAX_TREE_HPP
