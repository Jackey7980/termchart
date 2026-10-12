# 网络与关系图（graph/diagrams/）

## 已落地图

| 图 | 头文件 | Options | 类型 |
| --- | --- | --- | --- |
| 力导向节点-连线 | mindmap.hpp（NodeLinkOptions） | `NodeLinkOptions` | examples/20 |
| 桑基图 | sankey.hpp | `SankeyOptions` | `SankeyNode` / `SankeyFlow` |
| 弦图 | chord.hpp | 按头文件 | `ChordEntity` |
| 弧线图 | arc_diagram.hpp | 按头文件 | — |
| 邻接矩阵 | adjacency_matrix.hpp | 按头文件 | — |
| 社交网络 | social_network.hpp | `SocialNetworkOptions` | — |
| 知识图谱 | knowledge_graph.hpp | `KnowledgeGraphOptions` | — |

## 各图适用场景

- **力导向（node-link）**：通用关系网络，节点按力模拟散开
  （examples/20_force_graph；力导向算法在 `graph/layout/force.hpp`）；
- **桑基**：流量分配（源 → 中间 → 汇），`SankeyFlow` 携带流量值
  （examples/13_sankey）；
- **弦图 / 弧线图**：两两关系的环形呈现（关系密集时比力导向可读）；
- **邻接矩阵**：n×n 格矩阵，最紧凑、可精确查值（稠密图首选）；
- **社交网络 / 知识图谱**：带社区/实体类型语义的力导向变体。

## 布局选择速查

| 图特征 | 布局（graph/layout/） |
| --- | --- |
| 无向、关系密集 | force.hpp |
| 层级/树 | tree_layout.hpp |
| 中心放射 | radial.hpp / circular.hpp |
| 聚类分组 | cluster.hpp |
| 线路图 | metro.hpp |

## 路由与避让

- 直线/弧线边走 `graph/route/arc.hpp`、`spline.hpp`、`bundling.hpp`（边聚束）；
- 正交边走 `route.hpp` + `obstacle.hpp`（WireGrid 障碍）；
- 标签避让 `graph/label_place.hpp`；交叉计数（布局质量评估）
  `graph/crossing.hpp`。

## 示例

13_sankey、19_metro_map、20_force_graph。
