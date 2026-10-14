# 模块依赖矩阵

> **本文件由 `tools/gen_module_graph.py` 从 `cmake/TermchartModules.cmake` 生成，禁手改。**
> 内容同 `结构.md` 附一。

| 层号 | 模块 | 允许 include（层号更低的全部模块） |
| --- | --- | --- |
| 0 | core | — |
| 1 | util | ≤0 |
| 2 | color | ≤1 |
| 3 | scale | ≤2 |
| 4 | scene | ≤3 |
| 5 | image | ≤4 |
| 6 | raster | ≤5 |
| 7 | term | ≤6 |
| 8 | document | ≤7 |
| 9 | protocol | ≤8 |
| 10 | canvas | ≤9 |
| 11 | layout | ≤10 |
| 12 | chart | ≤11 |
| 13 | graph | ≤12 |
| 14 | render | ≤13 |
| 15 | interaction | ≤14 |
| 16 | backend | ≤15 |
| 17 | dsl | ≤16 |
| 18 | widget | ≤17 |
| 19 | plugin | ≤18 |
| 20 | app | ≤19 |
| 21 | api | ≤20 |
| 22 | gui | ≤21 |

判定顺序：禁止边（`结构.md` 附三）→ 收窄边（`结构.md` 附二）→ 层号（本表）。
由 `tools/check_layers.py` 逐条 `#include` 执行。
