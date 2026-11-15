# C ABI 与 Python 桥（termchart_capi）

`termchart_capi` 是 termchart 的**稳定 C ABI 适配层**：用不透明句柄 + POD 结构体 + `tc_value`
通用值载体，把 C++ 高层出图 API（`api::quick` 那一「一行出图」层）暴露给 C，再让语言绑定
（Python 已落地，Rust/Java 规划中）包壳。它不抛 C++ 异常过边界、不跨 ABI 传 `std::string`、
大对象导出走两段式缓冲——详见 [FFI / 语言桥方案](design/ffi_plan.md) §1。

设计约束决定了一条铁律：**C 头骨架与函数签名只增不改，新增配置项 = 加枚举值 + 一个 switch**，
已编译的调用方与语言绑定不受影响。

## 1. 构建与链接

`termchart_capi` 是一个静态库（`termchart_capi`），构建时连带核心 `termchart` 一起编出：

```sh
cmake --preset ninja
cmake --build --preset ninja --target termchart_capi
```

产物（路径因平台/生成器而异）：

- 头：`libs/capi/include/termchart_capi.h`
- 库：`<build>/libs/capi/libtermchart_capi.a`（MinGW）/ `termchart_capi.lib`（MSVC）
- 依赖：须同时链接核心库 `termchart`

C 侧使用：

```c
#include "termchart_capi.h"
/* 编译： -I libs/capi/include  -L <build>/libs/capi  -ltermchart_capi -ltermchart */
```

## 2. C 快速上手

```c
#include "termchart_capi.h"
#include <stdio.h>

int main(void) {
    tc_init();                                   /* 幂等初始化（可重复调用） */

    static const double x[] = {1,2,3,4,5};
    static const double y[] = {5,3,4,2,6};

    tc_chart_handle c = tc_chart_create_line();
    tc_chart_add_points(c, x, y, 5);

    tc_plot_opts opts = {0};                     /* 全部 0 = 取库默认 */
    opts.width = 60; opts.height = 16; opts.title = "demo"; opts.show_legend = 1;

    char* ansi = NULL;
    tc_error_handle err = NULL;
    if (tc_chart_render_ansi(c, &opts, &ansi, &err) != TC_OK) {
        char msg[256]; tc_error_message(err, msg, sizeof msg);
        fprintf(stderr, "render failed: %s\n", msg);
        tc_error_free(err);
    } else {
        printf("%s", ansi);                       /* UTF-8 + 真彩 SGR */
        tc_free_string(ansi);
    }
    tc_chart_free(c);
    return 0;
}
```

`tc_plot_opts` 里的 `width/height/title/show_legend/depth` 属于「展示」属性，按次传；
只有「模型」属性（域、刻度、零线、轴标签）走下面 §4 的属性通道。

## 3. 图表与系列

| 工厂 | 图种 | 喂数据 |
| --- | --- | --- |
| `tc_chart_create_line` | 折线 | `tc_chart_add_points` / `tc_chart_add_series` |
| `tc_chart_create_scatter` | 散点 | 同上 |
| `tc_chart_create_bar` | 柱状 | `tc_chart_add_bar`（值 + 类别标签）/ `tc_chart_set_values` |
| `tc_chart_create_pie` | 饼图 | `tc_chart_set_values`（值 + 类别标签） |
| `tc_chart_create_sparkline` | 迷你图 | `tc_chart_set_values` |

- **多系列**（折线/散点）：用 `tc_chart_add_series`，一次调用传 `count` 条、点集一次性构造，
  不经逐点 setter。每条系列可带 `name`（图例）与 `color`（`tc_color`，`kind=TC_COLOR_NONE`
  走图表默认色）。
- **不支持该图种的数据形状** → 显式 `TC_ERR_UNSUPPORTED`，不静默丢弃：例如给折线调
  `tc_chart_add_bar` 会报错；给只收系列的图种调 `tc_chart_set_values` 同样报错。

```c
tc_series s[2] = {
    { .name="sq",  .x=x, .y=y1, .n=5 },
    { .name="lin", .x=x, .y=y2, .n=5 },
};
tc_chart_add_series(c, s, 2, &err);              /* append 语义 */
```

## 4. 模型属性通道

配置项不再「一个配置一个函数」（那会让函数数 = 图种 × 配置项，且每加一项都要改头、改实现、
改每个语言绑定），而是收成一条通道：

```c
tc_value v = {0};                                 /* 零初始化 = 未设置 */
v.kind = TC_VAL_DOUBLE_ARRAY; v.p = (const double[]) {0, 18}; v.n = 2;
tc_chart_set_prop(c, TC_PROP_Y_DOMAIN, &v, &err); /* 折线/柱状：固定纵轴域 */
```

`tc_value` 的取值（`kind`）：

| kind | 字段 | 说明 |
| --- | --- | --- |
| `TC_VAL_NONE` | — | 清除该属性（恢复默认） |
| `TC_VAL_BOOL` | `i` | 0 / 非 0 |
| `TC_VAL_INT` | `i` | |
| `TC_VAL_DOUBLE` | `d` | |
| `TC_VAL_STRING` | `s`, `n` | UTF-8；`n=0` 时按 `strlen(s)` |
| `TC_VAL_DOUBLE_ARRAY` | `p`, `n` | 数组首地址 + 元素个数 |

属性表（**只增不改**；不支持该属性的图种返回 `TC_ERR_UNSUPPORTED`）：

| 属性 | kind | 适用图种 / 语义 |
| --- | --- | --- |
| `TC_PROP_Y_DOMAIN = 1` | DOUBLE_ARRAY(2) | 折线/柱状：`{lo, hi}`，要求有限且 `hi>lo`；柱状须自行把 0 并进域 |
| `TC_PROP_AUTO_DOMAIN = 2` | BOOL | 折线/散点（柱状恒自动域 → Unsupported） |
| `TC_PROP_INCLUDE_ZERO = 3` | BOOL | 折线/散点，仅 AUTO_DOMAIN 开启时生效 |
| `TC_PROP_EMPHASIZE_ZERO = 4` | BOOL | 仅折线：强调零线 |
| `TC_PROP_SHADE_NEGATIVE = 5` | BOOL | 仅折线：负区阴影 |
| `TC_PROP_X_TICKS = 6` | DOUBLE_ARRAY | 折线：显式 x 刻度（时间轴）；空数组恢复自动 |
| `TC_PROP_X_LABEL = 7` | STRING | **X 轴标签**（见 §5） |
| `TC_PROP_Y_LABEL = 8` | STRING | **Y 轴标签**（见 §5） |

## 5. 坐标轴标签

轴标签属 **Dataset 语义**，不在 C++ 核心 `Chart` 的渲染契约里——所以这一能力在 **C-ABI 层
后置绘制**，不污染 C++ 核心基类，ABI 也不变（只是多了两个 `tc_chart_prop` 枚举值）。

两条渲染路径各用各的方式把标签贴进「预留边距」，不与图本身重叠：

- **Canvas 路径（ANSI / PNG）**：用 `render::Compositor` 把「图」与「轴标签」分层合成到
  预留的上下/左右边距里，互不覆盖。
- **SVG 路径**：在矢量 `Scene` 里追加 `TextOp` 画标签；因为 `Scene::bounds()` 不统计 `Text`
  （autofit 的 `viewBox` 据此定），用**零长 `LineTo` 的极值角撑开包围盒**，标签就落进框内、
  不被裁。

> **已知约定**：Y 轴标题在 SVG 中逐字符**纵向排布**（SVG 无文本旋转）；Canvas（终端/位图）
> 中也是纵向堆叠。X 轴标签横向居中置于图下方。

C 示例：

```c
tc_value v = {0};
v.kind = TC_VAL_STRING; v.s = "Time";
tc_chart_set_prop(c, TC_PROP_X_LABEL, &v, &err);
v.s = "Value";
tc_chart_set_prop(c, TC_PROP_Y_LABEL, &v, &err);
```

Python 示例（见 §7）：

```python
c.set_prop("x_label", "Time")
c.set_prop("y_label", "Value")
```

清除标签：`set_prop` 传 `TC_VAL_NONE` 即可。

## 6. 导出 SVG / PNG

大对象导出走**两段式缓冲**——库不分配内存、不跨 ABI 传 `std::string`：

1. 先传 `cap=0`（buf 可为 `NULL`）→ `*out_len` 回写所需字节数；
2. 再按 `*out_len` 分配缓冲重新调用 → 写入并再次回写实际长度。

```c
tc_plot_opts opts = {0}; opts.width = 80; opts.height = 24;

uint64_t len = 0;
tc_chart_to_svg(c, &opts, NULL, 0, &len, &err);   /* 问长度 */
char* buf = malloc(len);
tc_chart_to_svg(c, &opts, buf, len, &len, &err);   /* 写 */
/* buf 现在是完整 SVG 文本（UTF-8） */
free(buf);
```

`tc_chart_to_png` 同款两段式，返回 PNG 字节流（链路全在核心内：
`Canvas → CellBuffer → gui::render_cells_to_bitmap → image::encode(Png)`，不牵任何 GUI
窗口后端）。`opts->width/height` 在 PNG 路径里表示**格数**（默认 80×24），每格像素与底色取
`gui::CellBitmapOptions` 默认（9×16、深色底）。

## 7. Python 绑定（`termchart_capi` 包）

基于本 C ABI 的纯 ctypes 封装，**无需编译扩展**（只要构建好 C 库、运行时能被加载即可）。
它的 API 与 C 一一对应：

```python
from termchart_capi import Chart

with Chart("line") as c:
    c.add_series([1, 2, 3, 4, 5], [5, 3, 4, 2, 6], name="a", color=(255, 140, 0))
    c.add_series([1, 2, 3, 4, 5], [2, 3, 5, 7], name="lin", color=6)  # 6 = ANSI16 cyan
    c.set_prop("y_domain", (0, 18))          # 模型属性（元组 → DOUBLE_ARRAY）
    c.set_prop("x_label", "Time")            # 字符串属性
    c.set_prop("y_label", "Value")

    print(c.ansi(width=60, height=16, title="demo", show_legend=True))
    open("out.svg", "w").write(c.to_svg(width=60, height=16))
    png: bytes = c.to_png(width=60, height=16)   # 返回 PNG 字节；传 path= 可直接落盘
```

要点：

- `Chart(kind)` 的 `kind` ∈ `line / scatter / bar / pie / sparkline`，对应 C 的工厂函数。
- `add_series(x, y, name=None, color=None)`：`color` 可为 RGB 三元组 `(r,g,b)` 或 ANSI16/256
  索引整数；`None` 走默认色。
- `set_prop(prop, value)`：
  - 模型属性（`y_domain` 等）传**元组/列表** → `TC_VAL_DOUBLE_ARRAY`；
  - 布尔属性（`auto_domain` 等）传 Python `bool` → `TC_VAL_BOOL`；
  - **字符串属性**（`x_label` / `y_label`）传 `str` → `TC_VAL_STRING`（非字符串会显式报错）。
  - 传 `None` → `TC_VAL_NONE`（清除）。
- 输出：`ansi(...)`（返回字符串）、`print(...)`（写 stdout）、`to_svg(...)`、`to_png(...)`
  （均可带 `path=` 直接落盘，否则返回内容/字节）。`width/height/title/show_legend` 为展示
  属性；`ansi/print` 另支持 `depth`（默认 `"truecolor"`）。
- 句柄式：用 `with` 或显式 `close()` 释放；同一对象可反复渲染。

> 运行时依赖：加载 `termchart_capi` 共享库（Windows 上 `libtermchart_capi.dll` 须在
> `PATH` 或包旁；其它平台为 `libtermchart_capi.so/.dylib`）。

## 8. 错误处理

失败一律走 `tc_error_handle` + `tc_status`（非 0 = 出错类别），**不从函数直接抛错**：

```c
tc_error_handle err = NULL;
tc_status st = tc_chart_to_png(c, &opts, buf, cap, &len, &err);
if (st != TC_OK) {
    char msg[256]; tc_error_message(err, msg, sizeof msg);
    /* 处理 msg */
    tc_error_free(err);
}
```

状态码（`tc_status_code`，**只增不改**）：

| 码 | 含义 |
| --- | --- |
| `TC_OK = 0` | 成功 |
| `TC_ERR_INVALID_HANDLE` | 句柄为空或已释放 |
| `TC_ERR_INVALID_ARG` | 参数非法（长度不符、域退化、字符串属性传错类型 …） |
| `TC_ERR_RENDER` | 数据 / 渲染阶段失败 |
| `TC_ERR_NOMEM` | 内存不足 |
| `TC_ERR_IO` | 写 stdout / 文件失败 |
| `TC_ERR_BUFFER_TOO_SMALL` | 调用方缓冲不足：`*out_len` 回写所需长度 |
| `TC_ERR_UNSUPPORTED` | 该图种 / 该属性不支持：显式报错，不静默降级 |

## 9. 设计取舍

- **属性通道而非逐函数**：C 没有重载 / 可选参数 / 变体，逐函数会让函数数 = 图种 × 配置项，
  且每加一项都要改头文件、改实现、改每个语言绑定——这正是「牵一发而动全身」。
  属性通道把「加配置项」收敛成「加一个枚举值 + 一个 case」。
- **轴标签在 C-ABI 后置绘制**：核心 `Chart` 的渲染契约不应为胶水层便利而膨胀；轴标签语义
  本就属 Dataset，放在 C-ABI 层用 Canvas/Scene 既有能力补齐，核心零改动。
- **两段式缓冲**：大对象（SVG/PNG 字节）由调用方分配、库只写入，避免跨 ABI 传 `std::string`
  与库侧内存管理。
- **ABI 稳定性承诺**：状态码、属性枚举只增不改；结构体字段顺序固定；已编译调用方按数值判断
  仍然成立。
