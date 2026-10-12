# UML 各图（graph/diagrams/uml_*）

## 已落地四图

| 图 | 头文件 | Options | 类型 |
| --- | --- | --- | --- |
| 类图 | uml_class.hpp | `UmlClassOptions` | — |
| 序列图 | uml_sequence.hpp | `UmlSequenceOptions` | `SeqParticipant` / `SeqMessage` / `SeqNote` / `SeqModel`、`SeqArrowKind` |
| 活动图 | uml_activity.hpp | `UmlActivityOptions` | — |
| 用例图 | uml_usecase.hpp | `UmlUsecaseOptions` | — |

## 类图

- 节点 = 类（名称 + 属性区 + 方法区），边 = 关系
  （关联/继承/实现/聚合/组合，样式经 `ArrowStyle`）；
- 属性/方法区在格粒度下用横线分隔的多行文本框表达。

## 序列图（模型与其他 UML 图不同）

序列图是**时间轴模型**而非节点-边模型，因此有独立的数据结构：

```cpp
struct SeqParticipant { /* 参与者 */ };
struct SeqMessage     { /* 消息：from/to/文本/箭头种类(SeqArrowKind) */ };
struct SeqNote        { /* 注释 */ };
struct SeqModel       { /* 参与者 + 消息 + 注释的序列 */ };
```

 DSL 侧对应 `dsl/mermaid_sequence.hpp` 的 `parse_sequence()`
（mermaid 总入口对 sequenceDiagram 返回 Unsupported——时间轴模型不进
AstGraph，这是刻意的，见 design 与 dsl_reference.md）。

## 活动图

流程控制语义（起止、判断分支、并发分叉汇合）；
判断节点轮廓与流程图共用 `ShapeKind` 体系。

## 用例图

参与者（小人）+ 用例（椭圆）+ 关联/包含/扩展边；
格粒度下小人用组合字符近似。

## 通用约定

- 全部走 `render_uml_xxx(Model&, Canvas&, Options)` 一步到位模式；
- Mermaid classDiagram / stateDiagram 可经 `dsl::parse_mermaid` → `AstGraph`
  → `to_graph()` 进入（examples/17_uml_class、18_uml_sequence）；
- 样式 Options 聚合布局/路由/节点/边样式子选项。

## 未做

- 组件图 / 部署图 / 对象图 / 包图（结构.md 的 UML 全集中未列出的剩余图种）；
- PlantUML 方言对 UML 的覆盖度以 `dsl/plantuml.hpp` 为准。
