# 入门

## 前置条件

- C++20 编译器（GCC 15.2 / Clang / MSVC 均可，零强制第三方依赖）。
- CMake + Ninja（Windows 上推荐 Ninja，GNU make 在含非 ASCII 的路径下不可靠）。

## 构建

```sh
cmake --preset ninja
cmake --build --preset ninja
ctest --preset ninja          # 全量测试
```

构建目录在 `build/<presetName>`。库本体是 header-first：`include/termchart/**` 全部可直接 include，
配套实现在 `src/**`（同路径同名），最小集成方式见 installation.md。

## 第一张图：一行出图

```cpp
#include "termchart/api/quick.hpp"

int main() {
    termchart::api::init_core();                    // 幂等；进程内一次性初始化
    termchart::api::hello();                        // 零配置彩色输出（examples/32）
}
```

## 自定义一行输出

```cpp
#include "termchart/api/quick.hpp"

termchart::api::Opts opts;
opts.width(60).color_depth(termchart::ColorDepth::Ansi256).ascii_only();
auto bytes = termchart::api::render_line("hello", termchart::Style{}, opts);  // 不写 stdout
termchart::api::print_stdout(bytes.value());
```

`Opts` 的每个 setter 都有对应 getter，未设置的项由库自行决定（见 `api/opts.hpp`）。

## 画带坐标轴的图表

数据装进 `chart::Dataset`，图表类消费它（完整列表见 charts/cartesian.md）：

```cpp
#include "termchart/chart/dataset.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"

termchart::chart::Dataset ds;
ds.add("sin", ys);                       // 只有 y 时 x 取 0..n-1
ds.set_x_label("t");

termchart::LineChart chart;
chart.set_title("sin");
chart.draw({canvas});                    // DrawContext{canvas}，开关默认全开
```

## 真实可跑的示例

`examples/` 下每个示例都有对应的 ctest 条目（`example_<name>`），常用的：

| 示例 | 内容 |
| --- | --- |
| 03_function_plot / 04_bar_chart | 带坐标轴的折线 / 柱状 |
| 09_radar / 10_gauge | 极坐标 |
| 14_flowchart / 15_state_machine / 16_mindmap | 图可视化 |
| 33_mermaid_flowchart | Mermaid 文本 → 图 |
| 34/35/36_export_* | SVG / HTML / JSON 导出 |
| 38_image_sixel | 终端显示位图 |
| 82_autoviz_csv | 地狱级脏 CSV → 自动选图（AutoViz） |

## 下一步

- 选图表：guide/choosing_chart.md
- 导出：guide/exporting.md
- 脏数据一键出图（探索性分析）：guide/autoviz.md
- 显示乱码或错位：reference/faq.md（Windows 中文环境与自适应尺寸是两大常见坑）
