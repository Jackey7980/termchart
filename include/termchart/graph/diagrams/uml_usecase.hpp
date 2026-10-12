// uml_usecase.hpp —— UML 用例图（参与者小人 + 椭圆用例 + 无箭头关联线）
//
// 字符化策略：参与者画成三行**小人**（O 头 / /|\ 身 / / \ 腿，名字在下），
// 用例画成**椭圆**（ShapeKind::Ellipse + 居中名）；参与者—用例之间连**无箭头**
// 直线（关联）。参与者站两翼：节点属性 "side" == "left"/"right" 显式指定，
// 缺省按边方向判（只作为 from → 左；只作为 to → 右；混合 → 左）。
//
// 模型约定（沿用 graph::Graph）：Node::attributes() 的 "kind" == "actor" 为
// 参与者，其余节点一律按用例画椭圆。
#ifndef TERMCHART_GRAPH_DIAGRAMS_UML_USECASE_HPP
#define TERMCHART_GRAPH_DIAGRAMS_UML_USECASE_HPP

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/shape/shape.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct UmlUsecaseOptions {
    LayoutOptions layout{};   // 用例框的最小尺寸与间距；参与者框高固定 4 行
    double h_gap = 6.0;       // 参与者列与用例列之间的间隔
    Style actor_style{};      // 小人与参与者名
    Style usecase_style{};    // 椭圆与用例名
    Style edge_style{};       // 关联线
};

// 就地布局并绘制用例图，返回所有元素外框的并集。空图返回零矩形。
// 布局从左往右排（左参与者列 → 用例列 → 右参与者列）；画布不够宽的部分被裁剪。
[[nodiscard]] TERMCHART_API Rect render_uml_usecase(Graph& graph, canvas::Canvas& canvas,
                                                    const UmlUsecaseOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_UML_USECASE_HPP
