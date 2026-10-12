# tests/fixtures —— 测试夹具

## 本仓库的夹具约定（重要）

**夹具即生成物**：需要"一份稳定的输入数据"时，本仓库的做法是**写生成器脚本 +
提交生成物 + 用 `--check` 守一致性**，而不是手工维护一份长期不变的样例。既有先例：

| 夹具 | 生成器 | 形态 |
| --- | --- | --- |
| `tests/golden/**/*.txt`（9 份） | `tools/gen_golden.py`（置 `TERMCHART_UPDATE_GOLDEN=1` 时由测试二进制写出） | 快照文本，禁止手改 |
| `tests/unit/image/test_png_samples.inc` | `tools/gen_png_samples.py` | `.inc` 文本头，内嵌字节数组，`#include` 进测试 |
| `include/termchart/detail/*.hpp`（6 份） | `tools/gen_*.py` | 数据表头，`check_generated` 保证无 diff |

**为什么 PNG 样本不放在这里**：它需要 256 行十六进制字节、且必须能被 C++ 直接
`#include`，所以按既有约定落成 `.inc` 文本头（见上表第二行）。二进制夹具不纳入
本目录，避免"看不懂、改不动、无法校验"的 blob 入库。

## 规格要求的夹具清单（`结构.md` 的 `tests/fixtures/` 段）

| 文件 | 用途 | 状态 |
| --- | --- | --- |
| `sample_data.hpp` | 通用数据系列样例（折线/柱/散点共用） | **待补** |
| `sample_graph.hpp` | 图模型样例（含环、孤立点、长标签等边界） | **待补** |
| `sample_mermaid.txt` | Mermaid 源文本（`dsl/` 解析输入） | **已有**：生成器 `tools/gen_dsl_samples.py`，`check_generated` 守一致性；覆盖 8 种节点形状 / 两种边标签 / 边链 / subgraph / 需忽略指令 |
| `sample_dot.txt` | Graphviz DOT 源文本 | **已有**：同生成器；覆盖 rankdir、含 `->` 的引号标签、形状、并列节点集 |
| `sample_plantuml.txt` | PlantUML 源文本 | **已有**：同生成器；覆盖类体、五种关系、多重度、被忽略的 note/skinparam |
| `sample.csv` | CSV 表格输入（`dsl`） | **已有**：同生成器；覆盖引号字段含分隔符与换行、混合列类型推断 |
| `sample_grapheme.txt` | 字素簇用例（组合记号、ZWJ、区域指示符） | **待补** |

补齐顺序建议：`sample_graph.hpp` / `sample_data.hpp`（不依赖空层，且能立刻给
chart/graph 测试复用）→ `sample_grapheme.txt` → `sample.csv` → 三个 DSL 文本
（等 `dsl/` 落地后再补，否则夹具没有消费者）。

## fuzz 的语料放哪

`tests/fuzz/` 的种子语料**刻意内嵌在 `fuzz_common.hpp::seed_corpus()` 里**，不落成本
目录的文件：

* `--self-test` 要能在 ctest 里零文件依赖地跑（路径、工作目录、编码都不该成为故障源）；
* 语料是"最容易踩边界的几类字节模式"（截断 UTF-8、孤立 ESC、被切断的 OSC/DCS、
  代理区码点、全 0xFF），改动频率低、体量小，内嵌反而更好审查。

需要"从文件复现崩溃"时，`tests/fuzz/<target> <file>` 直接吃任意路径的文件。
