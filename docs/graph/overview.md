# 图可视化总览（graph/）

## 分层结构

```
graph/graph.hpp          语义层：Graph（节点 + 有向边 + 邻接查询）
graph/attributes.hpp     Attributes（节点/边元数据）
graph/node|edge.hpp      Node / Edge（ArrowStyle 箭头风格）
graph/layout/            布局：layout/tree_layout/force/circular/radial/cluster/metro
graph/route/             路由：route/arc/spline/obstacle/bundling/junction
graph/shape/             形状轮廓：ShapeKind
graph/wiregrid.hpp       WireGrid：连线栅格（障碍登记 + 路径检查）
graph/label_place.hpp    标签避让
graph/port.hpp           PortSide（边锚点侧）
graph/arrow.hpp          箭头绘制
graph/crossing.hpp       交叉计数
graph/diagrams/          端到端图（21 个，见下）
```

## Graph 核心 API（graph.hpp 已核对）

```cpp
Graph g;
Node& n = g.add_node("label");          // id 从 1 自增（0 无效）；引用可能因插入失效
Node* p = g.find(id);
bool ok = g.remove_node(id);            // 连带删关联边
Edge* e = g.add_edge(from, to, "label"); // 端点必须已存在
g.remove_edge(from, to);
g.edges_of(id);  g.out_degree(id);  g.in_degree(id);
Rect bb = g.bounds();                    // 布局之后才有意义
```

布局写 `Node::set_box`、路由写 `Edge::set_route`，走 `nodes()/edges()` 的可变访问。

## 端到端图清单（graph/diagrams/，21 个）

| 类别 | 图 |
| --- | --- |
| 流程控制 | flowchart、state_machine、uml_activity、petri_net |
| 结构 | uml_class、er_diagram、orgchart、dependency、dag、syntax_tree |
| 关系 | mindmap、knowledge_graph、social_network、chord、arc_diagram、adjacency_matrix |
| 序列 | uml_sequence |
| 网络 | metro_map、sankey、circuit |
| 语法 | syntax_tree |

全部模式统一：`render_xxx(Graph&/Model&, Canvas&, XxxOptions{}) → Rect`
（就地布局并绘制，返回整图包围盒；超画布部分由 canvas 裁剪）。

## 推荐用法

只想出图 → 直接用 diagrams（guide/graph_diagrams.md）；
要定制布局/路由 → 分步调用 layout + route + shape。

## 示例

14_flowchart、15_state_machine、16_mindmap、17_uml_class、18_uml_sequence、
19_metro_map、20_force_graph、13_sankey。
