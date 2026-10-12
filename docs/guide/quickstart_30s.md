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

## 4. 速查

| 想要 | API |
| --- | --- |
| 幂等初始化 | `api::init_core()` |
| 字节流（headless/测试） | `api::render_line(text, style, opts)` |
| 渲染并写出 | `api::print_line(text, style, opts)` |
| 写字节流 | `api::write_stdout / write_to / flush_stdout` |
| 零配置演示 | `api::hello(opts)` |

`Opts`：`width/height/color_depth/unicode/ascii_only/newline/reset_at_end`，
全部有默认值，未设项由库决定。

## 5. 然后呢

- 画真图表 → guide/first_chart.md
- Windows 下乱码 → reference/faq.md
- 完整示例 → examples/32_quick_one_liner.cpp
