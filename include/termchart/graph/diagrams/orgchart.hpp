// orgchart.hpp —— 组织架构图（层级树 + 共享树干 + ├/└ 接点）
//
// 与思维导图的区别（两者都用树）：
//   * 思维导图把根摆在中间、子树左右分列，连线按序号错开成"扇形"；
//   * 组织架构图是**单向层级**：深度沿 x 向右增长，同父的兄弟共用一条**树干**
//     （一条竖线），每个子节点从树干上"分叉"出去，接点画成 `├` / `└` / `┌`
//     而不是把直角拆成两个独立字符 —— 这正是终端里 `tree` 类工具的形状，
//     在字符网格上比逐条正交路由（互相穿插的 Z 形）好读得多。
//
// 节点内容就是 `Node::label()`（居中一行）。要显示"姓名 + 职务"这类两行内容，
// 请把职务并进标签（例如 "CTO / 平台"）——本图刻意不做第二行，避免与边框争夺行数。
#ifndef TERMCHART_GRAPH_DIAGRAMS_ORGCHART_HPP
#define TERMCHART_GRAPH_DIAGRAMS_ORGCHART_HPP

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/tree_layout.hpp"
#include "termchart/graph/shape/shape.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct OrgChartOptions {
    TreeOptions tree{};   // 节点尺寸、行距（深度向右，行距即兄弟间距）
    ShapeKind shape = ShapeKind::Box;
    Style node_style{};
    Style edge_style{};
    bool node_border = true;  // false：只画标签与连线
};

// 就地布局 + 路由 + 绘制；返回所有节点外框的并集。空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect render_orgchart(Graph& graph, canvas::Canvas& canvas,
                                                 const OrgChartOptions& options = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_ORGCHART_HPP
