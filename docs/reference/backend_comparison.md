# 后端对比与三物种

## 总表

| 后端 | 物种 | 输入 | 输出 | 头文件 |
| --- | --- | --- | --- | --- |
| ansi | snapshot | CellBuffer | ANSI 转义彩色文本 | backend/snapshot/ansi_backend.hpp |
| text | snapshot | CellBuffer | 纯文本 | backend/snapshot/text_backend.hpp |
| html_pre | snapshot | CellBuffer | HTML `<pre>`（保终端外观） | backend/snapshot/html_pre_backend.hpp |
| json | snapshot | CellBuffer | 格数据的 JSON | backend/snapshot/json_backend.hpp |
| svg | vector | Scene | SVG | backend/vector/svg_backend.hpp |
| html | structured | Document | 语义 HTML | backend/structured/html_backend.hpp |
| markdown | structured | Document | Markdown | backend/structured/markdown_backend.hpp |
| （TikZ） | vector | Scene | — | 规格有、未落地 |

## 三物种说明

| 物种 | 保留什么 | 丢什么 | 适合 |
| --- | --- | --- | --- |
| snapshot | 字符格 + 颜色 | 几何 | 终端外观复现、CI 快照、日志 |
| vector | 几何 + 样式 | 字符格粒度 | 无损缩放、网页嵌入、出版 |
| structured | 语义块 | 呈现细节 | 报告生成、富文本导出 |

三者输入模型互斥，后端**不做跨渲染模型转换**（backend.hpp 铁律）。
选型从数据源头决定：格化结果 → snapshot；矢量场景 → vector；语义文档 → structured。

## 共同接口

```cpp
backend.begin();      // 清空内部缓冲
backend.render(input); // 物种子接口定义输入类型
backend.end();        // 收口
backend.bytes();      // 结果视图（不跨下一次 begin/render 保存）
// 便捷：render_snapshot / render_vector / render_structured
```

## 选项

- `SnapshotOptions{depth=真彩, trim_trailing_blank=true}`
- `VectorOptions{scale=12.0, autofit=true, width=640, height=480}`
- `StructuredOptions{title, css_class="termchart"}`

## 约束

- `backend > chart` 禁止边：任何后端不认识图表类型；
- ansi snapshot 是唯一放行 include `render/sgr.hpp` 的导出路径（收窄边白名单）；
- vector 后端消费 Scene 时 Fill/Stroke 状态指令语义必须与 canvas 同构。

## 示例

examples/34_export_svg、35_export_html、36_export_json。
