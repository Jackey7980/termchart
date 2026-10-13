# 30 秒上手

## 1. 构建（约 20 秒增量）

```sh
cmake --preset ninja && cmake --build --preset ninja
```

## 2. 一行出图

```cpp
#include "termchart/api/quick.hpp"
int main() {
    termchart::api::init_core();
    termchart::api::hello();     // 零配置彩色输出
}
```

## 3. 常用变体

```cpp
termchart::api::Opts o;
o.width(40).ascii_only();                          // 40 列、纯 ASCII
termchart::api::print_line("hello", {}, o);        // 渲染 + 写 stdout
```

```cpp
auto s = termchart::api::render_line("hello", {}, {});  // 只要字节、不写 stdout
```

## 3.5 一行出图（**给图表**，不只是给文本）

```cpp
#include "termchart/api/chart.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"

termchart::chart::LineChart chart;
chart.set_title("sine");
termchart::chart::Series s;
s.name = "sin(x)";
for (int i = 0; i <= 80; ++i) s.points.push_back({i / 8.0, std::sin(i / 8.0)});
chart.add_series(std::move(s));

termchart::api::Opts o;
o.width(80).height(24);
termchart::api::print_chart(chart, o);   // 渲染 + 写 stdout
// 只想要字节（headless / 测试 / 内嵌）：api::render_chart(chart, o)
```

`render_chart` 把"建 Canvas → draw → render_full → 写 stdout"四步样板收成一次调用；
尺寸不自动探测终端（同一个 Chart 在 CI 与真终端里必须产出同样字节），
要自适应请显式传 `term::query_size()` 的结果。

## 4. 速查

| 想要 | API |
| --- | --- |
| 幂等初始化 | `api::init_core()` |
| 字节流（headless/测试） | `api::render_line(text, style, opts)` |
| 渲染并写出 | `api::print_line(text, style, opts)` |
| **图表 → 字节流** | `api::render_chart(chart, opts)` |
| **图表 → 渲染并写出** | `api::print_chart(chart, opts)` |
| 写字节流 | `api::write_stdout / write_to / flush_stdout` |
| 零配置演示 | `api::hello(opts)` |

`Opts`（全库统一叫 `*Options`，`api::Opts` 是 `ApiOptions` 的兼容别名）：
`width/height/color_depth/unicode/ascii_only/newline/reset_at_end`，
全部有默认值，未设项由库决定。`render_chart` 的显式尺寸 `0` 是
`InvalidArgument`（不猜也不钳位，与 `render_line` 同一口径）。

## 5. 然后呢

- 画真图表 → guide/first_chart.md
- Windows 下乱码 → reference/faq.md
- 完整示例 → examples/32_quick_one_liner.cpp
