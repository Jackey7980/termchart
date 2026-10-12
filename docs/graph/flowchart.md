# 流程图（graph/diagrams/flowchart.hpp）

## API（已核对）

```cpp
struct FlowchartOptions {
    LayoutOptions layout{};           // graph/layout/layout.hpp
    RouteOptions route{};             // graph/route/route.hpp
    RouteStyle route_style = RouteStyle::Orthogonal;
    ShapeKind shape = ShapeKind::Box; // graph/shape/shape.hpp
    Style node_style{};
    Style edge_style{};
};
Rect render_flowchart(Graph& graph, canvas::Canvas& canvas,
                      const FlowchartOptions& opts = {});
```

就地布局并绘制到画布，返回整图包围盒；超出画布的部分由 canvas 裁剪。

## 组合管线（固定）

```
分层布局（layout）→ 路由（正交/直线）→ 绘制形状、标签、箭头
```

需要单独控制某一步时，直接调用 graph/layout、graph/route、graph/shape。

## 节点形状

`ShapeKind` 决定轮廓（矩形/圆角/菱形等，枚举值见 graph/shape/shape.hpp）。
格粒度下曲线轮廓有台阶是预期行为。

## 路由约束（实现层踩过的坑，调用方须知）

1. **正交路由的 Z 方向按节点框相对位置选**（x 分离 → 横向优先）；
   按锚点距离选会让折线撞上源框边框列；
2. **节点框必须先登记进 `WireGrid` 当障碍再路由**（diagrams 入口已自动做；
   分步调用时自己负责），否则折线穿节点；
3. 同侧多条边按 `(节点, 侧)` 分组均匀分配锚点，共用同格会退化成斜线。

## 从 Mermaid 来

```cpp
auto ast = termchart::dsl::parse_mermaid(source);   // flowchart 方言 → AstGraph
termchart::graph::Graph g = ast.value().to_graph();
termchart::graph::render_flowchart(g, canvas);
```

完整示例：examples/14_flowchart（分层布局 + 正交路由 + 形状/标签/箭头）、
examples/33_mermaid_flowchart。

## 样式

`node_style` / `edge_style` 直接是 `core::Style`；空 Style 用库默认配色。
箭头风格在 `graph/edge.hpp` 的 `ArrowStyle`。
