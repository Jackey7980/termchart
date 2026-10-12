# 布局算法对比（graph/layout/）

## 已落地布局

| 布局 | 头文件 | 原理 | 适用 | 代价 |
| --- | --- | --- | --- | --- |
| 分层 | layout.hpp（`LayoutOptions`） | 拓扑分层 + 层内排序 | DAG/流程图/依赖 | O(V+E) 级 |
| 树 | tree_layout.hpp | 递归子树打包 | 树/思维导图/组织架构 | O(V) |
| 力导向 | force.hpp | 斥力 + 引力迭代 | 无向网络 | O(iter·E)，有收敛参数 |
| 环形 | circular.hpp | 等距摆圆 | 轮换关系、小图 | O(V) |
| 径向 | radial.hpp | 树 + 极坐标放射 | 层次放射（思维导图变体） | O(V) |
| 聚类 | cluster.hpp | 分组包裹 | 社区结构 | 按头文件 |
| 地铁 | metro.hpp | 线路对齐 | 交通线路图 | 按头文件 |

## 选择决策树

```
图有方向性（依赖/流程）？
├─ 是 → 是树吗？
│       ├─ 树 → tree_layout
│       └─ DAG → 分层布局
└─ 否 → 节点 < 20 且想强调环形关系？
        ├─ 是 → circular / radial
        └─ 否 → 力导向（有社区语义再叠 cluster）
```

## 布局质量

- `graph/crossing.hpp`：交叉计数——布局质量的客观度量，分层布局的层内排序
  以减少交叉为目标；
- `graph/label_place.hpp`：标签避让（布局后处理）；
- 力导向的收敛：`bench/bench_force` 盯收敛性；迭代数/阈值在 Options
  （以 force.hpp 为准）。

## 布局后必须走路由

布局只定**节点框**；边的形状由 `graph/route/` 决定：

| 边样式 | 路由 |
| --- | --- |
| 正交折线 | route.hpp（`RouteStyle::Orthogonal`）+ obstacle（WireGrid 障碍绕行） |
| 直线 | route.hpp（直线样式） |
| 弧线 | arc.hpp |
| 样条 | spline.hpp |
| 聚束 | bundling.hpp |

路由铁律：节点框先登记障碍；Z 方向按节点框相对位置选（见 design/graph
相关坑位记录与 guide/graph_diagrams.md）。

## 性能基线

`bench/bench_sugiyama`（分层扩展性）、`bench/bench_force`（收敛）——
`--quick` 进 ctest 冒烟。

## 未做

- Kamada-Kawai 布局（结构.md 提及，未落地）；
- 障碍感知的全局样条路由（obstacle 目前服务正交路由）。
