# 事件系统

## 决策

`interaction/` 用 `std::variant` 表达事件，五类（`interaction/event.hpp`）：

```cpp
enum class EventKind : std::uint8_t { Key, Mouse, Resize, Focus, Paste };
using Event = std::variant<KeyEvent, MouseEvent, ResizeEvent, FocusEvent, PasteEvent>;
```

- variant 的 `index()` 即 EventKind，枚举顺序与 variant 参数顺序一一对应；
- `kind_of(event)` / `kind_name(kind)` 是 constexpr 工具。

## 分发模型

`interaction/dispatcher.hpp`：**订阅顺序即优先级，被消费即停**。

```cpp
Dispatcher d;
d.subscribe(EventKind::Mouse, [](const Event& e) { /* 返回 true = 已消费 */ });
d.subscribe_all(handler);          // 兜底/全局观察者
bool consumed = d.dispatch(event);
```

Handler 签名 `std::function<bool(const Event&)>`，返回 true 表示消费，冒泡终止。

## 上游输入

- `interaction/input_parser.hpp`：ANSI/UTF-8/SGR 鼠标字节流 → 事件（增量解析器，
  喂字节出事件）；
- `interaction/mouse.hpp` / `key.hpp`：`MouseEvent`（坐标 + 按键 + 修饰）/ `KeyEvent`；
- `term/mouse_mode.hpp`：`MouseMode` / `MouseEncoding`（终端侧开关与编码协议）。

## 命中测试

`interaction/hit_test.hpp` + `chart/hit_region.hpp`：chart 侧登记 `HitRegion`
（Rect/Circle/Polygon + 不透明 payload id），交互层只按形状做几何判定并回报 id。
**hit_test 只 switch HitShape，禁图表名**——interaction 不认识任何具体图表类型。

## 交互控件（interaction/ 下的独立能力）

pan / zoom（`Viewport`）/ tooltip / crosshair / highlight / select / brush /
legend_toggle / focus / immediate（即时模式辅助）。它们都是消费 Event、操作
View/Canvas 的独立组件，不依赖具体图表。

## 约束

- `render > interaction` 是禁止边（渲染器不做输入）；
- 终端状态（raw mode、鼠标开关）收在 `term/Terminal`，interaction 不直接改 tty 设置。
