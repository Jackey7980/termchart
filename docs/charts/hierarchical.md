# 层次图族（chart/hierarchical/，7 个 + 节点模型）

## 已落地图表

| 图表 | 类 | 头文件 | Options |
| --- | --- | --- | --- |
| 树图 | `Treemap` | treemap.hpp | `TreemapOptions` |
| 冰柱图 | `Icicle` | icicle.hpp | `IcicleOptions` |
| 日晷图 | `Sunburst` | sunburst.hpp | `SunburstOptions` |
| 树状图 | `Dendrogram` | dendrogram.hpp | `DendrogramOptions` |
| 圆填充 | `PackChart` | pack.hpp | `PackOptions` |
| 树视图 | `TreeView` | tree_view.hpp | `TreeViewOptions` |
| （节点模型） | `HierNode` | node.hpp | 层次数据的通用节点 |

## 数据模型

`HierNode`（chart/hierarchical/node.hpp）是层次数据的通用节点：
树形结构（父/子），带名称与量值；六个图表全部消费它。
构造一次 `HierNode` 树，可分别喂给 treemap / icicle / sunburst 对比呈现。

## 布局思想

- **Treemap**：递归切分矩形（按量值比例），叶节点 = 矩形块；
- **Icicle**：根在上，层级向下延伸，宽度按量值比例；
- **Sunburst**：Icicle 的极坐标版（环层）；
- **Dendrogram**：二叉/多叉树的连接图呈现；
- **Pack**：兄弟圆内切父圆；
- **TreeView**：缩进行式树（纯文本导航友好）。

## 字符化约定

- 区块边界用制表符/框线字符（Unicode）或 ASCII 退化（`ascii_only`）；
- 区块内密度字符（█▓▒░）按量值深浅表达；
- 标签放不下时截断/省略，不溢出区块。

## 示例

examples/12_treemap。

## 与 graph/ 的关系

层次"图"（节点连线树）走 graph/（tree_layout、mindmap、orgchart、dendrogram
是两个不同呈现）：本族是**区域填充**呈现（treemap/icicle/sunburst），
graph/ 是**节点-连线**呈现。同一棵树两种讲法，按读者任务选。
