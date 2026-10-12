# Scene：非完备图形 IR

## 定位

`scene::Scene` 是**绘制指令序列**（矢量 IR），三条渲染路径在它这里分岔：

1. 终端实时：Scene → `canvas/scene_renderer` → `Canvas` → `to_buffer()` → CellBuffer → Renderer；
2. vector 导出：Scene → `backend/vector/*`（svg；规格还含 TikZ）；
3. structured：Scene 被 `document/block.hpp` 的 Chart 块持有，走 structured 导出。

## "非完备"的边界（刻意的）

`scene/command.hpp` 顶部明文规定：只放 vector 后端与 canvas **都能**消费的图元。
不要往里塞 canvas 才有的能力——渐变填充、裁剪路径、图层合成属于 `canvas/`（L10）。

指令集（聚合体 + `std::variant`）：

| 指令 | 字段 | 说明 |
| --- | --- | --- |
| `MoveTo` / `LineTo` | `Point to` | 路径 |
| `RectOp` | `Rect rect` | 轴对齐矩形 |
| `ArcOp` | center/radius/start/end | 弧度制，逆时针；整圆 = {0, 2π} |
| `FillOp` / `StrokeOp` | Color（+ width） | **状态指令，只作用于其后出现的图元**（与 SVG 同构） |
| `TextOp` | at + 文本 | 内容 |

## 关键设计点

- **聚合体 + variant**：`index()` 即指令类别；每个聚合体显式 `operator== = default`
  （C++20 聚合体不隐式获得 ==），于是 Scene 可整体比较——golden 与 roundtrip 测试依赖它。
- **不保存路径栈**："当前路径"由消费者（canvas / vector 后端）维护；保存它等于把
  canvas 的状态机搬进 IR，正是要避免的事。
- **两个承诺**：可顺序回放、可整体比较。它不理解颜色、不做裁剪、不算布局。
- `bounds()` 只统计带几何的指令；Text 不参与（宽度要等字体度量，不是 IR 该知道的事）。
- **状态指令语义必须同构实现**：SVG 等 vector 后端按同样规则（Fill/Stroke 作用于
  之后的图元），否则 `几何 → Stroke` 会着色错位（CODEBUDDY.md 坑 13）。
- 新增指令 = 往 variant 加一个聚合体，所有访问者会被编译期逼着补全。

## 收口约定（R12）

`canvas/scene_renderer.cpp` 描边走 braille 2×4、填充走 halfblock 1×2，叠加只覆盖
非空格；盲文空白是 `U+2800` 不是空格，判空两者都要跳。
