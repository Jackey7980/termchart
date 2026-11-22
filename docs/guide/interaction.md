# 交互开发

## 组件清单（interaction/，全部已落地）

| 头文件 | 能力 |
| --- | --- |
| input.hpp / input_parser.hpp | 输入字节流 → 事件（ANSI/UTF-8/SGR 鼠标解析） |
| event.hpp | `Event` = variant<Key/Mouse/Resize/Focus/Paste> |
| dispatcher.hpp | 事件分发（订阅顺序即优先级，消费即停） |
| hit_test.hpp | 命中测试（配合 chart/hit_region.hpp） |
| spatial_index.hpp | `HitIndex` 空间哈希索引（同一批区域反复查询时 opt-in） |
| focus.hpp | 焦点链 |
| pan.hpp / zoom.hpp | 平移 / 缩放（`Viewport` + `ViewportOptions`） |
| tooltip.hpp / crosshair.hpp | 提示框（`Tooltip` + `TooltipPlacement`）/ 十字线 |
| highlight.hpp / select.hpp / brush.hpp | 高亮 / 选择 / 框选 |
| legend_toggle.hpp | 图例开关 |
| immediate.hpp | 即时模式辅助 |

## 最小事件循环

```cpp
termchart::term::Terminal term({.raw_mode = true, .alt_screen = true,
                                .hide_cursor = true, .enable_mouse = true});
term.enter();                                  // RAII，析构自动回滚

termchart::interaction::Dispatcher dispatch;
dispatch.subscribe(termchart::interaction::EventKind::Key, [&](const Event& e) {
    // 处理按键；返回 true 表示已消费
    return true;
});
dispatch.subscribe_all([&](const Event& e) {   // 兜底观察者
    return false;
});

termchart::interaction::InputParser parser;    // 增量喂字节、吐事件
// 循环：读 stdin 字节 → parser.feed → 每个 Event 交给 dispatch
```

## 命中测试

chart 侧登记 `HitRegion`，交互层只按形状判定并回报 payload：

```cpp
using termchart::chart::HitRegion;
regions.push_back(HitRegion::rect(bounds, payload_id));
regions.push_back(HitRegion::circle(center, radius, payload_id));
regions.push_back(HitRegion::polygon(points, payload_id));
// hit_test 对 (col,row) 求 contains；命中即把 payload 交给你解释
```

约束：hit_test 只 switch `HitShape`（Rect/Circle/Polygon），禁图表名——
interaction 不认识任何具体图表类型。

### 同一批区域反复查询：HitIndex

`topmost_hit` / `hit_payloads` 每次都是 O(N) 线性扫描，适合一次性判定。
悬停、鼠标连续移动这类「建一次、查多次」的场景可换用 `HitIndex`：

```cpp
#include "termchart/interaction/spatial_index.hpp"

termchart::interaction::HitIndex index(regions);   // 内部持有快照
// 鼠标移动：查询近似 O(1)，语义与线性版完全一致
const auto* hit = index.topmost(x, y);
const auto ids = index.payloads(x, y);             // 自下而上
index.reset(new_regions);                          // 区域变了就重建
```

- `topmost` 取最后登记者、`payloads` 自下而上，与 `hit_test.hpp` 逐位一致；
- 返回指针指向索引内部快照，调用方不得跨 `reset()` 保存；
- 超大/退化区域自动挂到溢出表，仍恒被检查，不会漏命中；
- 查询点含 NaN/Inf 时退回线性语义，行为与一次性版本相同。

## 典型组合

- 悬停 tooltip：Mouse 事件 → hit_test → `Tooltip` 定位渲染
  （examples/21_hover_tooltip）；
- 缩放平移：滚轮/拖拽 → `Viewport` 记录变换 → 重画
  （examples/22_zoom_pan）；
- 图例开关：点击图例区（登记为 HitRegion）→ `legend_toggle` 翻转系列可见性
  （examples/30_legend_toggle）。

## 终端侧配合

- 鼠标开关与编码：`term/mouse_mode.hpp`（`MouseMode` / `MouseEncoding`），
  由 `TerminalOptions::enable_mouse` 在 `enter()` 时启用；
- 尺寸变化：`ResizeEvent`（POSIX 侧经 `term/signal.hpp` 的 SIGWINCH）；
- 注意差分渲染：每帧后 `Renderer::render(buffer)` 只输出变化部分；
  清屏后先 `reset()`。
