# DAG 与依赖图（graph/diagrams/dag.hpp、dependency.hpp）

## 已落地图

| 图 | 头文件 | Options |
| --- | --- | --- |
| DAG | dag.hpp | `DagOptions` |
| 依赖图 | dependency.hpp | `DependencyOptions` |

## DAG（dag.hpp）

有向无环图的标准呈现：**分层布局**（拓扑层从上/左到下）+ 正交/直线路由。

```cpp
termchart::graph::Graph g;
g.add_edge(a, b);              // a → b：b 依赖 a（箭头方向按图定）
termchart::graph::render_dag(g, canvas, {});   // 签名以头文件为准
```

适用：任务调度依赖、构建依赖、数据血缘。

## 依赖图（dependency.hpp）

包/模块依赖的专用呈现：节点 = 包（带名称），边 = 依赖方向；
呈现上强调方向性（箭头 + 分层）。适合直接对接构建系统的输出
（如 CMake 的依赖 JSON / dot 文本，经 `dsl/graphviz_dot.hpp` 解析进入）。

## 从 DOT 来

```cpp
#include "termchart/dsl/graphviz_dot.hpp"
// dot 文本 → AstGraph → to_graph() → render_dag / render_dependency
```

Graphviz DOT 是描述 DAG 的通用交换格式，建议外部工具链经此进入。

## 环检测

`render_dag` 对含环输入的行为以头文件注释为准（分层布局要求无环；
调用方应自行保证或先用拓扑排序校验）。

## 与 network.md 的分工

DAG/依赖图是**分层方向性**呈现；network（力导向等）是**无向/关系**呈现。
构建依赖选本篇；社交/知识网络选 network。

## 示例

examples/14_flowchart 的分层布局同源（flowchart 本质也是 DAG 呈现）。
