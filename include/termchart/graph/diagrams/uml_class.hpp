// uml_class.hpp —— UML 类图（三栏类框 + 空心三角泛化 + 正交连线）
//
// 字符化策略：类框画成**三栏矩形**——类名居中、属性/操作左对齐，两栏之间用
// ├─┤ 分隔线（UML 分类格在字符网格上的等价表达，画法同 examples/17_uml_class）；
// 泛化边（默认）按正交折线绘制，并在**基类（from）端的边框格**上画空心三角
// `◁`（UML ▷ 语义指向基类）；边属性 "kind" == "assoc" 的边退化为普通箭头。
//
// 模型约定（沿用 graph::Graph，无需新容器）：
//   * Node::label() 是类名；
//   * Node::attributes() 的 "attrs" / "ops" 分别是属性栏与操作栏内容，
//     多行以 '\n' 连接，每行自带 UML 可见性前缀（+ 公开 / - 私有 / # 保护）。
#ifndef TERMCHART_GRAPH_DIAGRAMS_UML_CLASS_HPP
#define TERMCHART_GRAPH_DIAGRAMS_UML_CLASS_HPP

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/shape/shape.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct UmlClassOptions {
    LayoutOptions layout{};  // min_width/min_height 会被成员内容自动撑大
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Orthogonal;
    Style border_style{};   // 类框边框与分隔线
    Style name_style{};     // 类名
    Style member_style{};   // 属性与操作
    Style edge_style{};     // 连线与三角/箭头
    bool generalization_triangles = true;  // false：泛化也画普通箭头
};

// 就地布局、路由并绘制类图，返回所有类框的并集。空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect render_uml_class(Graph& graph, canvas::Canvas& canvas,
                                                  const UmlClassOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_UML_CLASS_HPP
