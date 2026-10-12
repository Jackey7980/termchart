# 画第一张图

## 最短路径

```cpp
#include "termchart/api/quick.hpp"
int main() {
    termchart::api::init_core();
    termchart::api::hello();
}
```

编译链接 `src/**.cpp`（或按 installation.md 集成），跑起来即见彩色输出。

## 折线图（完整链路）

数据 → `Dataset` → 图表类 → `DrawContext{canvas}` → `Canvas` → 输出：

```cpp
#include "termchart/chart/dataset.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/canvas/canvas.hpp"

termchart::canvas::Canvas canvas(w, h);          // 尺寸用 terminal.size() 自适应
termchart::chart::Dataset ds;
ds.add("sales", {1.0, 3.0, 2.0, 5.0, 4.0});      // 只有 y：x 取 0..n-1
ds.add("cost", xs, ys);                          // 或 x/y 等长成对
ds.set_y_label("万元");

termchart::LineChart chart;
chart.set_title("季度销售");
chart.set_margins(1, 1, 3, 8);                   // 默认即此值
chart.draw(termchart::chart::DrawContext{canvas});
```

`DrawContext` 开关（默认全开）：`draw_title / draw_axes / draw_grid / draw_legend`；
`grid_style / axis_style` 为空时用库默认（网格暗灰、轴默认前景）。

## 柱状 / 散点 / 其他直角坐标图

API 形态统一（`struct XxxOptions` + `class XxxChart final : public Chart`，
`set_*` 配置 + `draw(ctx)`）。全列表见 charts/cartesian.md。

## 极坐标 / 统计 / 层次…

| 图族 | 入口头文件目录 |
| --- | --- |
| 饼/环/雷达/仪表 | chart/polar/ |
| 直方图/箱线/ROC | chart/statistical/ |
| 树图/冰柱/日晷 | chart/hierarchical/ |
| KPI/进度环 | chart/indicator/ |

每族文档见 charts/ 目录。

## 三个必守约定

1. **画布自适应**：宽取 `cols`、高取 `rows - 1`，按 1:2 宽高比取景，
   否则折行 × 滚动会让图错位；
2. **Color 随系列走**：`Dataset::add(name, values, color)` 的色参与图例一致；
3. **终端太保守时**：`Opts::ascii_only()` 全 ASCII 退化，`ColorDepth::Ansi16`
   强制 16 色。

## 验证

跑 `examples/03_function_plot`、`examples/04_bar_chart`（ctest 条目
`example_03_function_plot` 等）对照真实输出。
