# 状态机（graph/diagrams/state_machine.hpp）

## API

```cpp
struct StateMachineOptions { /* 见头文件：布局/路由/样式子选项 */ };
Rect render_state_machine(Graph& graph, canvas::Canvas& canvas,
                          const StateMachineOptions& opts = {});
```

（签名以头文件为准；模式与 flowchart 一致：就地布局 + 绘制 + 返回包围盒。）

## 语义约定

- 节点 = 状态；边 = 转移（边上标签 = 触发事件）；
- 起止标记：初始状态用入边标记（● →），终止状态用双圈/外框表达——
  转移事件标签 + 起止标记是本图的差异化呈现（examples/15_state_machine）；
- 自转移（状态到自身）的环路由由 route 层处理，标签避让走
  `graph/label_place.hpp`。

## 构建方式

### 手工建图

```cpp
termchart::graph::Graph g;
auto& idle = g.add_node("Idle");
auto& run  = g.add_node("Running");
g.add_edge(idle.id(), run.id(), "start");
g.add_edge(run.id(), idle.id(), "stop");
termchart::graph::render_state_machine(g, canvas);
```

### Mermaid 来

```cpp
// stateDiagram-v2 方言 → AstGraph → to_graph()
auto ast = termchart::dsl::parse_mermaid(source);
termchart::graph::Graph g = ast.value().to_graph();
```

## 布局

默认走分层布局（状态机天然有方向性：初始态在顶层）。
环状转移多时考虑配合 `graph/route/obstacle.hpp` 的绕行参数。

## 示例

examples/15_state_machine（转移事件标签 + 起止标记）。
