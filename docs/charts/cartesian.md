# 直角坐标图族（chart/cartesian/，20 个）

## 已落地图表

| 图表 | 类 | 头文件 | 一句话 |
| --- | --- | --- | --- |
| 折线 | `LineChart` | line_chart.hpp | 连线 • + 数据点 ● |
| 柱状 | `BarChart` | bar_chart.hpp | █ 柱体 |
| 面积 | `AreaChart` | area.hpp | 折线 + 填充 |
| 阶梯 | `StepChart` | step.hpp | 阶梯折线 |
| 散点 | `ScatterChart` | scatter_chart.hpp | 点标记 |
| 气泡 | `BubbleChart` | bubble.hpp | 点大小编码第三维 |
| 直方图 | `HistChart` | hist.hpp | 分箱计数 |
| 箱线 | `BoxPlotChart` | boxplot.hpp | 五数概括 |
| 小提琴 | `ViolinChart` | violin.hpp | 分布轮廓 |
| 脊线 | `RidgelineChart` | ridgeline.hpp | 多组分布重叠 |
| 热力图 | `HeatmapChart` | heatmap.hpp | 格粒度色/字符密度 |
| 等高线 | `ContourChart` | contour.hpp | 等值线 |
| 瀑布 | `WaterfallChart` | waterfall.hpp | 增量瀑布 |
| 子弹 | `BulletChart` | bullet.hpp | 目标 vs 实际 |
| 误差棒 | `ErrorBarChart` | errorbar.hpp | 带误差的点 |
| 哑铃 | `DumbbellChart` | dumbbell.hpp | 两点连线比较 |
| 甘特 | `GanttChart` | gantt.hpp | 时间区间条 |
| 迷你线(格内) | `SparklineChart` | cartesian/sparkline.hpp | 单格系列 |
| 平行坐标 | `ParallelChart` | parallel.hpp | 多维竖轴 |
| 马赛克 | `MosaicChart` | mosaic.hpp | 二维比例块 |
| QQ | `QQChart` | qq.hpp | 分位-分位 |
| 流图 | `StreamgraphChart` | streamgraph.hpp | 堆叠流 |

迷你线另有独立轻量类 `chart::Sparkline`（chart/sparkline.hpp，非 Chart 体系）。

## 统一 API 形态

```cpp
struct XxxOptions { /* 语义参数 */ };
class XxxChart final : public termchart::chart::Chart {
public:
    void set_xxx(...);                 // 数据与选项的 set_*
    void draw(termchart::chart::DrawContext ctx) override;
};
```

基类提供 `set_title` / `set_margins`（默认 1/1/3/8）；
公共几何工具（`data_bounds` / `make_area` / `draw_legend_for`）在 chart/chart.hpp。

## 字符化约定

- 折线类：`•` 连线、`●` 数据点（chart.hpp 常量）；
- 柱/填充类：`█` 及密度字符系列（█▓▒░）；
- 系列/扇区靠**专属字符**区分（单色终端可读），颜色是叠加信息；
- 轴/网格/图例由 `DrawContext` 开关 + `layout/` 实现，不自拼。

## 示例

examples/03_function_plot、03_line_chart、04_bar_chart、04_heatmap、02_sine_wave。
