# 思维导图（graph/diagrams/mindmap.hpp）

## API（类型名已核对）

```cpp
struct NodeLinkOptions { /* 节点-连线视图共用选项 */ };
struct MindmapOptions   { /* 思维导图专用选项 */ };
// 渲染入口签名以头文件为准（render_mindmap 模式：就地布局 + 绘制 + 返回 Rect）
```

mindmap 头同时提供 `NodeLinkOptions`——节点-连线视图（examples/20_force_graph
的呈现方式）与思维导图共享一套节点连线绘制基础设施。

## 语义约定

- 根在中心/左侧，子树向两侧/右侧展开（树布局）；
- 分支连线走**扇形连线**（tree 布局的弧形连接，examples/16_mindmap）；
- 节点文本即标签，标签避让走 `graph/label_place.hpp`。

## 数据来源

### 手工建树

```cpp
termchart::graph::Graph g;
auto& root = g.add_node("主题");
auto& a = g.add_node("分支A");
g.add_edge(root.id(), a.id());
```

（树形数据的另一种建法：直接用 `chart/hierarchical/node.hpp` 的 `HierNode`
配 `graph/layout/tree_layout.hpp` 分步绘制。）

### Mermaid 来

Mermaid 的 mindmap 方言解析支持以 `dsl/` 头文件清单为准
（mermaid.hpp 目前识别 flowchart/sequence/class/state 四种，
mindmap 方言未在入口枚举中——需要时经 AstGraph/DOT 路径转换）。

## 布局

`graph/layout/tree_layout.hpp`（树布局）与 `graph/layout/radial.hpp`
（径向布局，中心放射）可切换；扇形连线由 tree 布局的连接绘制产生。

## 示例

examples/16_mindmap（树布局 + 扇形连线）。

## 与 hierarchical/ 的分工

mindmap 是节点-连线呈现；treemap/icicle/sunburst 是区域填充呈现。
同一棵树两种讲法。
