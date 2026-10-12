# Document IR

## 定位

`document::Document` 是 backend/structured（HTML / Markdown）的输入：它只表达**语义**
（段落/标题/代码/表格/图表），不含任何渲染或像素决策。

```
Document（块列表） ──→ backend/structured/html_backend      → HTML
                  └─→ backend/structured/markdown_backend   → Markdown
```

## 结构

- `document/document.hpp`：`Document` = 块容器。`add_block()`、`blocks()`、
  `kind_of(i)`、`to_plain_text()`（最小纯文本序列化，给测试与 text 导出用）。
- `document/block.hpp`：`Block`（variant）+ `BlockKind`。图表以 **Scene 块**被持有
  ——Scene 是矢量 IR，Document 只引用它，不复制几何。
- `document/span.hpp`：行内富文本跨度（样式化文本段）。
- `document/table_model.hpp`：表格语义模型（structured 表格导出的输入）。

## 为什么 backend 不认识 chart

`backend > chart` 是禁止边。Document 提供的语义块让结构化导出成立：
后端看到的是"这里有一张图（Scene）/一张表（TableModel）"，而不是
"这里有一个 LineChart"。于是：

- 新增图表类型零改动 structured 后端（可扩展性契约实验之一）；
- 图表 → 文档 → 导出 的链路里，chart 模块不需要出现。

## 边界

- Document 不做布局、不决定 HTML 类名（`StructuredOptions{title, css_class}` 由
  导出方给）；
- `to_plain_text()` 是测试/降级用的最小序列化，不是完整后端——真正的 HTML/Markdown
  在 `backend/structured/*`；
- Scene 与 Document 的分工：Scene 管"怎么画"（几何），Document 管"有什么"（语义块）。
