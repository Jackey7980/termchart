# 后端抽象与三物种

## 三物种

后端的铁律（`backend/backend.hpp` 顶部）：**不做跨渲染模型转换**。输入是什么由
子接口在类型上钉死：

| 物种 | 子接口 | 输入 | 实现 | 输出 |
| --- | --- | --- | --- | --- |
| snapshot | `backend/snapshot/SnapshotBackend` | `CellBuffer` | ansi / text / html_pre / json | ANSI 转义文本 / 纯文本 / HTML pre / JSON |
| vector | `backend/vector/VectorBackend` | `scene::Scene` | svg（`svg_backend` + `svg_entities`） | SVG；规格另含 TikZ |
| structured | `backend/structured/StructuredBackend` | `document::Document` | html / markdown | HTML / Markdown |

统一生命周期（`backend::Backend` 基类）：`begin()` 清空内部缓冲 → `render(input)` →
`end()` 收口；`bytes()` 返回结果视图（不得保存跨下一次 begin/render）。
每个物种有便捷函数：`render_snapshot()` / `render_vector()` / `render_structured()`。

## 选项

- `SnapshotOptions{depth, trim_trailing_blank}`：depth 只影响 ansi 后端；
  trim 裁掉行尾"空白且无样式"的格。
- `VectorOptions{scale=12.0, autofit=true, width=640, height=480}`：
  1 场景单位 → 多少输出单位；autofit 用 `Scene::bounds()` 自动定 viewBox。
- `StructuredOptions{title, css_class="termchart"}`。

## 为什么三物种、为什么不做转换

- snapshot 输入是格化结果：无几何信息，只能表达"字符格 + 颜色"；
- vector 输入保留几何：可无损缩放（SVG viewBox）；
- structured 输入是语义块：能生成有结构的 HTML/Markdown。
三者输入模型互斥，任何"跨模型转换"都会在某一边丢失信息或伪造信息，
所以被类型系统 + 禁止边（`backend > chart`、收窄边 `ansi_backend > render/sgr`）
双重挡住。

## SGR 约定

ansi snapshot 后端是唯一被放行 include `render/sgr.hpp` 的导出路径（收窄边白名单）；
其余后端不生成终端转义。

## 未做

- vector 物种的 TikZ 后端（规格有，头文件未见）；
- structured 物种的 LaTeX 等其他格式。
