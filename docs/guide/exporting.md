# 导出：SVG / HTML / JSON / Markdown

## 三物种对照

| 想要什么 | 物种 | 后端 |
| --- | --- | --- |
| ANSI 彩色文本快照 | snapshot | `backend/snapshot/ansi_backend.hpp` |
| 纯文本 | snapshot | `backend/snapshot/text_backend.hpp` |
| HTML `<pre>`（终端外观） | snapshot | `backend/snapshot/html_pre_backend.hpp` |
| JSON（格数据） | snapshot | `backend/snapshot/json_backend.hpp` |
| SVG（矢量） | vector | `backend/vector/svg_backend.hpp` |
| HTML（语义文档） | structured | `backend/structured/html_backend.hpp` |
| Markdown | structured | `backend/structured/markdown_backend.hpp` |

## 用法（vector 物种为例）

```cpp
#include "termchart/scene/scene.hpp"
#include "termchart/backend/vector/svg_backend.hpp"

termchart::backend::vector::SvgBackend svg;
termchart::backend::vector::VectorOptions opt;   // scale=12, autofit=true
opt.autofit = true;                              // 用 Scene::bounds() 定 viewBox
svg.set_options(opt);
auto out = termchart::backend::vector::render_vector(svg, scene); // begin+render+end
```

snapshot / structured 同构：`render_snapshot(backend, buffer)`、
`render_structured(backend, doc)`。

## 选项

- `SnapshotOptions{depth=真彩, trim_trailing_blank=true}`：depth 只影响 ansi 后端；
- `VectorOptions{scale=12.0, autofit=true, width=640, height=480}`：
  autofit=false 时用给定画布尺寸；
- `StructuredOptions{title, css_class="termchart"}`。

## 完整链路

- 图表 → 导出：chart 画进 `Canvas` → `to_buffer()` → snapshot 后端；
  或 chart 场景走 `Scene` → vector 后端；
- 文档 → 导出：`Document`（段落/表格/图表块）→ structured 后端；
- 契约保证：后端只消费 IR，不认识图表类型（`backend > chart` 禁止边）——
  新图表类型零改动导出。

## 示例

examples/34_export_svg、35_export_html、36_export_json。

## 未做

- vector 物种的 TikZ（规格有，实现未见）；
- LaTeX / PDF 类导出。
