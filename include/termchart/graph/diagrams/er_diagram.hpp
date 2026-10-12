// er_diagram.hpp —— ER 图（实体表框 + 鸭掌记号基数 + 关系名标签）
//
// 字符化策略：实体画成**表框**——实体名居中一行、├─┤ 分隔、字段左对齐（关系
// 模式即"表"）；关系按正交折线绘制，两端用**鸭掌记号**（crow's foot）标注基数，
// 记号画在紧贴实体边框的那段线上：
//   * 恰一（One）     → 两道垂直短杠 `││`
//   * 多（Many）      → 鸭掌 `<` `>` `∧` `∨`（按进框方向选）
//   * 零或一（0..1）  → 圆圈 `○` + 一道短杠 `│`
// 关系名作边标签，用 label_place 避让实体框（放不下宁可不画）。
//
// 模型约定（沿用 graph::Graph）：
//   * Node::label() 是实体名；Node::attributes() 的 "fields" 是字段清单，
//     多行以 '\n' 连接；
//   * Edge::label() 是关系名；Edge::attributes() 的 "card_from" / "card_to"
//     是两端基数，取值 "1"（恰一）、"N"/"many"/"*"（多）、"0..1"（零或一），
//     缺省按恰一处理。
#ifndef TERMCHART_GRAPH_DIAGRAMS_ER_DIAGRAM_HPP
#define TERMCHART_GRAPH_DIAGRAMS_ER_DIAGRAM_HPP

#include <cstdint>
#include <string_view>

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

enum class ErCardinality : std::uint8_t {
    One = 0,       // 恰一：两道短杠
    Many,          // 多：鸭掌
    ZeroOrOne,     // 零或一：圆圈 + 短杠
};

// 基数拼写 → 枚举；无法识别时按恰一（One）。
[[nodiscard]] TERMCHART_API ErCardinality parse_er_cardinality(std::string_view text) noexcept;

struct ErDiagramOptions {
    LayoutOptions layout{};  // min_width/min_height 会被实体内容自动撑大
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Orthogonal;
    Style entity_style{};   // 实体框与分隔线
    Style name_style{};     // 实体名
    Style field_style{};    // 字段
    Style edge_style{};     // 关系线、鸭掌与关系名
    bool draw_cardinality = true;
};

// 就地布局、路由并绘制 ER 图，返回所有实体框的并集。空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect render_er_diagram(Graph& graph, canvas::Canvas& canvas,
                                                   const ErDiagramOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_ER_DIAGRAM_HPP
