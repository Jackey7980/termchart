# 流向图族（chart/flow/）

## 已落地图表

| 图表 | 类 | 头文件 | Options |
| --- | --- | --- | --- |
| 漏斗 | `Funnel` | funnel.hpp | 按头文件为准 |
| 流向图 | `StreamChart` | stream.hpp | `StreamOptions{centered}` |

## stream.hpp（已核对的真实 API）

```cpp
struct StreamOptions {
    bool centered = true;  // true: ThemeRiver 中心对称；false: 自底向上堆叠
};
class StreamChart final : public Chart {
public:
    void set_series(std::vector<Series> series);  // 各系列点数必须一致
    void draw(DrawContext ctx) override;
    StreamOptions options;
};
```

头文件注释要点：规格原定 flow/stream 是"转发 cartesian/streamgraph"的同层语义
包装，但落地时 streamgraph 未就绪，故 flow/stream **先自给堆叠算法**
（各列按占比从基线向另一侧堆叠 █），待 cartesian/streamgraph 落地后可改回纯转发。
布局不依赖 graph/。

## stream vs streamgraph（规格强调的区分）

- `chart/flow/stream.hpp`（`StreamChart`）：ThemeRiver 语义包装，
  centered 开关控制中心对称或自底向上；
- `chart/cartesian/streamgraph.hpp`（`StreamgraphChart`）：直角坐标流图本体。
二者语义重叠，最终以 streamgraph 为主实现、stream 为兼容包装
（见头文件注释的迁移计划）。

## 漏斗 Funnel

过程转化率呈现：每层宽度按占比递减，层间标注转化率。
字符化沿用密度字符系列。

## 桑基图去哪了

桑基在 **graph/diagrams/sankey.hpp**（`SankeyNode`/`SankeyFlow`/`SankeyOptions`），
不走 chart/ 体系——桑基本质是带流量的网络图，归图可视化。
见 docs/graph/network.md。examples/13_sankey。

## 数据约定

- StreamChart：各系列点数必须一致（按 y 值逐列堆叠，x 仅作对齐索引）；
- 非有限值按 0 处理。
