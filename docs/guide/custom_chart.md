# 写自定义图表

## 契约

继承 `chart::Chart`（chart/chart.hpp），实现 `draw(DrawContext)`（Canvas 直绘回退）与 `to_scene(scene::Scene&, width, height)`（**首选**，产出矢量 IR）：

```cpp
#include "termchart/chart/chart.hpp"

class MyChart final : public termchart::chart::Chart {
public:
    void draw(termchart::chart::DrawContext ctx) override;
};
```

基类已提供：`title_`（`set_title`）、四边 margin（`set_margins`，默认 1/1/3/8）。
共享工具（chart.hpp 内自由函数）：`data_bounds()`（系列范围）、
`make_area()`（由 margin + 范围构造 `layout::PlotArea`）、
`draw_legend_for()`；字符常量 `kLineChar('•')/kMarkerChar('●')/kBarChar('█')`。

## draw 里做什么

```cpp
void MyChart::draw(termchart::chart::DrawContext ctx) {
    auto& canvas = ctx.canvas;
    double xmin, xmax, ymin, ymax;
    termchart::chart::data_bounds(series_, xmin, xmax, ymin, ymax);
    auto area = termchart::chart::make_area(canvas, margin(), /*…*/);
    if (ctx.draw_axes)  { /* layout::draw_axis_x/y */ }
    if (ctx.draw_grid)  { /* 网格线 */ }
    // 你的字符化：canvas.draw_text / draw_line / 直接 put
    if (ctx.draw_legend) termchart::chart::draw_legend_for(canvas, series_, area, true);
}
```

- `DrawContext`（chart/context.hpp）的开关默认全开，尊重它们是图表的共同礼仪；
- 坐标映射用 `scale/`（`LinearScale` / `LogScale` + `nice`/`ticks`），
  不要手写映射；
- 轴/图例/标题用 `layout/`（draw_axis_x/y、draw_legend、title）——不要自拼。

## 字符化原则

- 格粒度解析落格，不做假精度（参考 polar/pie.hpp 顶部注释：逐格归一化半径 ≤ 1
  才落笔）；
- **系列可辨不靠颜色**：给每个系列/扇区专属填充字符（█▓▒░■●▲◆○ 循环），
  颜色是叠加信息；
- 非有限输入不崩溃：如 Progress 对 fraction 收敛到 [0,1]、非有限按 0 处理。

## to_scene：产出 Scene IR（首选路径）

现代图种优先实现 `to_scene(scene::Scene& out, std::uint32_t width, std::uint32_t height)`：
把图表描述成矢量指令（MoveTo/LineTo/StrokeOp/TextOp/GroupOp），而不是往字符格落笔。
同一份 Scene 会被 `canvas/scene_renderer` 收口成盲文/半块字符图、被 `backend/vector`
（SVG/PDF/TikZ）收口成矢量文件——不再是「先画完字符再转录」。

```cpp
bool MyChart::to_scene(scene::Scene& out, std::uint32_t width,
                       std::uint32_t height) const {
    // 网格/轴/标题走 TextOp（与内置图种同构）；
    // 数据几何用 out.begin_path()/move_to()/line_to()/stroke()，
    //   用 begin_group(id) 包出动画组（组 id 须与 animation_groups() 对齐）。
    return true;   // 返回 true = 已迁移；默认实现返回 false，调用方回退到 draw()
}
```

- `to_scene` 与 `draw` 共用同一套布局（`make_area(width, height, …)` 尺寸版），两版不可能算出不同结果；
- `draw(DrawContext)` 仍是基类纯虚、**必须实现**，作为 Canvas 直绘回退；
- 渲染入口统一走 `api::print_chart` / `api::chart_to_scene` / `api::render_chart`（`api/chart.hpp`），
  不要再自己拿 `Canvas` 往 stdout 写。

## 注册进注册表

```cpp
#include "termchart/chart/registry.hpp"
termchart::chart::ChartRegistry::instance().register_chart(
    "my_chart", [] { return std::make_unique<MyChart>(); });
```

时机：**App 构造前**（构造期冻结快照，见 design/registry.md）。
注册工厂而非原型，`create()` 每次给新实例。

## 交互支持（可选）

想被悬停/点击命中：登记 `chart::HitRegion`（rect/circle/polygon + payload id）。
interaction 层只按形状判定，不认识你的图表类型。

## 验证

- 新头文件要同步：`src/` 同名 cpp（虚表锚点）、单元测试、（如 header-only）
  `cmake/header_only.txt` 登记 + 重跑 `generate_all`；
- 跑 `check_all`（分层 + 每头独立编译 + 生成物一致）。
