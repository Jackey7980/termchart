# 图可视化指南

## 一图流

```
Graph（节点/边）→ 布局 → 路由 → 画形状/标签/箭头 → Canvas
```

## 第一步：建图

```cpp
#include "termchart/graph/graph.hpp"
termchart::graph::Graph g;
auto& a = g.add_node("开始");
auto& b = g.add_node("处理");
g.add_edge(a.id(), b.id(), "下一步");   // 端点必须已存在
```

注意：`add_node` 返回的引用可能在后续插入后失效，长期持有用 `id()` + `find()`。

## 第二步：选端到端入口（推荐）

`graph/diagrams/` 提供一步到位的 `render_xxx(Graph&, Canvas&, Options)`：

| 图 | 头文件 |
| --- | --- |
| 流程图 | flowchart.hpp |
| 状态机 | state_machine.hpp |
| 思维导图 | mindmap.hpp |
| 组织架构 | orgchart.hpp |
| 桑基图 | sankey.hpp（SankeyNode/SankeyFlow） |
| UML 类/序列/活动/用例 | uml_class/uml_sequence/uml_activity/uml_usecase.hpp |
| ER 图 | er_diagram.hpp（ErCardinality） |
| 地铁图 | metro_map.hpp（MetroLine） |
| 依赖图 / DAG | dependency.hpp / dag.hpp |
| 弦图 / 弧线图 / 邻接矩阵 | chord.hpp / arc_diagram.hpp / adjacency_matrix.hpp |
| 社交网络 / 知识图谱 | social_network.hpp / knowledge_graph.hpp |
| Petri 网 / 电路 / 语法树 | petri_net.hpp / circuit.hpp / syntax_tree.hpp |

每个 Options 聚合了布局/路由/样式子选项，如
`FlowchartOptions{layout, route, route_style, shape, node_style, edge_style}`。

## 第三步（可选）：分步控制

组合关系固定为：分层布局 → 路由（正交/直线）→ 绘制。需要单独控制时直接调用：

- `graph/layout/`：layout.hpp（`LayoutOptions`）、tree_layout.hpp、force.hpp、
  circular.hpp、radial.hpp、cluster.hpp、metro.hpp；
- `graph/route/`：route.hpp（`RouteStyle`/`RouteOptions`）、arc.hpp、spline.hpp、
  obstacle.hpp、bundling.hpp、junction.hpp；
- `graph/shape/shape.hpp`：`ShapeKind` 形状轮廓；
- `graph/wiregrid.hpp`：连线栅格（节点先登记为障碍再路由）。

## 已知路由约束（踩过的坑）

1. **Z 方向按节点框相对位置选**（x 分离就横向优先），不按锚点距离；
2. **节点框先登记进 WireGrid 当障碍再路由**，路径检查跳过首末格；
3. 同侧多条边按 `(节点, 侧)` 分组均匀分配锚点，否则共用同格退化成斜线。

## 与 DSL 的衔接

Mermaid flowchart / classDiagram / stateDiagram 解析为 `dsl::AstGraph`，
`to_graph()` 落到 `graph::Graph` 再走上述管线（examples/33_mermaid_flowchart）。
见 guide/dsl_reference.md。

## 完整示例

14_flowchart、15_state_machine、16_mindmap、17_uml_class、18_uml_sequence、
19_metro_map、20_force_graph（力导向 + 节点-连线视图）、13_sankey。
