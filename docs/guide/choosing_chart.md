# 如何选图表

## 按数据形态选

| 数据形态 | 推荐 | 头文件 |
| --- | --- | --- |
| y 随 x 连续变化 | 折线 `LineChart` / 面积 `AreaChart` / 阶梯 `StepChart` | chart/cartesian/ |
| 类别间比较 | 柱状 `BarChart` / 瀑布 `WaterfallChart` / 子弹 `BulletChart` | chart/cartesian/ |
| 两变量分布 | 散点 `ScatterChart` / 气泡 `BubbleChart` | chart/cartesian/ |
| 单变量分布 | 直方图 `HistChart` / 小提琴 `ViolinChart` / 箱线 `BoxPlotChart` / 脊线 `RidgelineChart` | chart/cartesian/ |
| 占比构成 | 饼 `PieChart` / 环 `DonutChart` / 堆叠流 `StreamChart` | chart/polar/, chart/flow/ |
| 多维指标对比 | 雷达 `RadarChart` / 平行坐标 `ParallelChart` | chart/polar/, chart/cartesian/ |
| 热力/密度 | 热力图 `HeatmapChart` / 等高线 `ContourChart` | chart/cartesian/ |
| 层次数据 | 树图 `Treemap` / 冰柱 `Icicle` / 日晷 `Sunburst` / 树状图 `Dendrogram` | chart/hierarchical/ |
| 时间区间 | 甘特 `GanttChart` | chart/cartesian/ |
| 金融序列 | K线 `CandlestickChart` / 砖形 `Renko`（chart/financial/） | chart/financial/ |
| 过程转化 | 漏斗 `Funnel` / 桑基（graph/diagrams/sankey） | chart/flow/, graph/diagrams/ |
| 地理分区 | 填色地图 `Choropleth` / 点密度 `DotDensity` / 六边形 `Hexbin` | chart/geo/ |
| 单值读数 | 仪表盘 `GaugeChart` / KPI 卡 `GaugeKpi` / 进度环 `ProgressRing` | chart/polar/, chart/indicator/ |

## 统计专用（chart/statistical/）

ROC / ECDF / 回归 / 相关系数矩阵 / Bland-Altman / 森林图 / PCA / 自相关 /
季节分解 / 生存曲线——数据是统计量或需要统计变换时从这里选。

## 判断顺序建议

1. **读者要回答什么问题**（趋势 → 折线；比较 → 柱状；构成 → 饼/堆叠）；
2. **数据维度**（2 维直角坐标、多维雷达/平行、层次树图、网络图）；
3. **终端宽度**（窄终端优先 sparkline / indicator，见下）。

## 小空间专用

- `chart::Sparkline`（chart/sparkline.hpp）：一行内的迷你线；
- `chart::Progress`（chart/text/progress.hpp）：横向进度条组；
- `chart::StatusLight` / `StarRating` / `Thermometer`（chart/indicator/）：状态指示。

## 相似图族的区分（规格强调）

- **gauge vs gauge_kpi**：polar/gauge 是弧形刻度带 + 指针（图形读数）；
  indicator/gauge_kpi 是边框 + 标签 + 主值 + 变化量的数字卡片（读数强调）。
- **progress vs progress_ring**：text/progress 是横向条形组（多值并列）；
  indicator/progress_ring 是单值环形读数。二者服务不同场景，不互相替代。
- **stream vs streamgraph**：flow/stream（ThemeRiver 语义，居中或自底向上堆叠）；
  cartesian/streamgraph（直角坐标流图）。详见 charts/flow.md。

## 易错选择

- 单色终端需要靠形状区分系列：所有图表都有专属字符策略，但饼图最依赖它——
  若图例不可用，考虑柱状替代饼图；
- 数据点极多时散点会糊：先考虑热力图或六边形聚合。
