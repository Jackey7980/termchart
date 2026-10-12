#!/usr/bin/env python3
"""gen_dsl_samples.py —— 生成 tests/fixtures/ 下的 DSL 输入样例（夹具即生成物）。

本仓库的夹具约定（tests/fixtures/README.md）：需要"一份稳定的输入"时，
写生成器 + 提交产物 + 用 --check 守一致，而不是手工维护样例文件。
`sample_mermaid.txt` 由本脚本产出，单测与 examples/33 都读它。

样例刻意覆盖：声明/引用顺序、两种边标签写法、边链、子图、六边形/胶囊/菱形/圆、
以及"认得出但不做"的指令（classDef / click / style）——它们必须被静默跳过而非报错。
只使用 mermaid_flow.hpp 明确列出的支持子集：这里出现什么形状，解析器就必须认什么。

`sample_dot.txt` / `sample_plantuml.txt` / `sample.csv` 同样由本脚本产出：
四个文件各自覆盖对应方言「已声明支持」的那部分语法，方言支持面缩水时
`tests/unit/dsl/test_dsl_fixtures.cpp` 会红。
"""
from __future__ import annotations

import sys

from _genutil import ROOT, emit, wants_check

FIXTURES = ROOT / "tests" / "fixtures"

SAMPLE_MERMAID = """%% sample_mermaid.txt —— 由 tools/gen_dsl_samples.py 生成，禁手改。
%% 用途：dsl/flowchart 解析器的稳定输入（单测 + examples/33）。
%% 覆盖点见生成器脚本注释：形状齐全 / 两种边标签 / 边链 / 子图 / 需忽略的指令。
graph LR
    classDef todo fill:#f66
    A[读取输入] --> B{校验}
    B -->|通过| C([写出结果])
    B -- 失败 --> D[报错]
    D --> E{可重试?}
    E -->|是| A
    E -->|否| F[人工介入]
    C --> G[日志]
    G --> H((结束))
    G --> I{{批量模式}}
    I --> J>异步队列]
    J --> H
    subgraph 重试阶段
        D
        E
        F
    end
    click A "https://example.com/read"
    style C fill:#0f0
"""


SAMPLE_DOT = """// sample_dot.txt —— 由 tools/gen_dsl_samples.py 生成，禁手改。
// 覆盖点：rankdir、带引号且含 "->" 的标签（属性按原文切片读，不能被箭头误伤）、
// 形状映射、带属性边、并列节点集展开、无向图语法、以及被忽略的默认属性行。
strict digraph Pipeline {
    rankdir=LR
    node [shape=box]
    a [label="read -> parse", shape=ellipse]
    b [label="parse"]
    c [label="validate", shape=diamond]
    d [label="sink", shape=cylinder]
    a -> b
    b -> c [label="ok", style=dashed]
    b -> {d e} -> f
    c -- d [dir=both]
    e [label="旁路"]
}
"""

SAMPLE_PLANTUML = """' sample_plantuml.txt —— 由 tools/gen_dsl_samples.py 生成，禁手改。
' 覆盖点：包裹、类体逐行保留、五种关系语义（含 <|-- 的方向反转）、多重度、标签、
' 以及必须被静默跳过的 note / skinparam。
@startuml Pipeline
    skinparam monochrome true
    class Reader {
        +open(path)
        -buffer
    }
    class Parser as "Parser 核心"
    interface Source
    Source <|-- Reader
    Reader "1" *-- "*" Parser : parses
    Parser o-- Token : yields
    Parser ..> Error : may raise
    note right of Reader : 需要先 open 再 parse
@enduml
"""

SAMPLE_CSV = """id,name,score,active,joined,note
1,Alice,91.5,true,2026-01-04,"hello, world"
2,Bob,78,no,2026-02-11,"multi
line note"
3,Carol,,yes,2026-03-02,
4,Dave,-4.25,false,2026-04-19,plain
"""


def main(argv: list[str]) -> int:
    check = wants_check(argv)
    changed = emit(FIXTURES / "sample_mermaid.txt", SAMPLE_MERMAID, check)
    changed |= emit(FIXTURES / "sample_dot.txt", SAMPLE_DOT, check)
    changed |= emit(FIXTURES / "sample_plantuml.txt", SAMPLE_PLANTUML, check)
    changed |= emit(FIXTURES / "sample.csv", SAMPLE_CSV, check)
    return 1 if changed and check else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
