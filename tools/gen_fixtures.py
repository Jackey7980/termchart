#!/usr/bin/env python3
"""gen_fixtures.py —— 生成 tests/fixtures/ 下的三份待补夹具（夹具即生成物）。

补齐 `tests/fixtures/README.md`「规格要求的夹具清单」里剩下的三件：

  * `sample_data.hpp`   —— 通用数据系列样例（折线 / 柱 / 散点共用）
  * `sample_graph.hpp`  —— 图模型样例（含环、孤立点、长标签、宽字符、平行边）
  * `sample_grapheme.txt` —— 字素簇用例（组合记号、ZWJ、区域指示符等）

为什么是生成器而不是手写：README 的仓库约定是「写生成器 + 提交产物 + 用
--check 守一致」。前两份是常量表，数值由等差数列 / 三角函数算出，手写必然漂移；
第三份含组合记号与 ZWJ 这类肉眼不可见的码点，手写极易被编辑器"规范化"掉。

消费方：`tests/unit/test_fixtures.cpp`（同时守夹具自身的不变量，例如图的边
必须指向存在的节点、字素簇切分必须能还原原文）。
"""
from __future__ import annotations

import math
import sys

from _genutil import ROOT, emit, wants_check

FIXTURES = ROOT / "tests" / "fixtures"

N = 12  # 序列长度：够画折线，也能在单测里一眼数完


def fmt(x: float) -> str:
    """定点输出，避免 repr 产生 1.0000000000000002 这类噪声。"""
    s = f"{x:.6f}".rstrip("0").rstrip(".")
    return s if s not in ("", "-0") else "0"


def arr(values) -> str:
    return ", ".join(fmt(v) for v in values)


X = [float(i) for i in range(N)]
LINEAR = [2.0 * i + 1.0 for i in range(N)]
SINE = [math.sin(i / 2.0) for i in range(N)]
COUNTS = [3.0, 7.0, 5.0, 11.0, 9.0, 13.0, 8.0, 6.0, 12.0, 4.0, 10.0, 2.0]
LABELS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
# 散点：带负值与重复 x，用来压 chart 的边界处理
SCATTER = [(-2.0, 4.0), (-1.0, 1.0), (0.0, 0.0), (0.5, 0.25),
           (1.0, 1.0), (1.5, 2.25), (2.0, 4.0), (2.5, 6.25)]

# ── sample_graph.hpp 的数据 ──────────────────────────────────────────────────
GRAPH_NODES = [
    ("a", "读取输入"),
    ("b", "解析"),
    ("c", "校验"),
    ("d", "写出结果"),
    ("iso", "孤立节点（度为 0）"),
    ("wide", "宽字符标签"),
    ("verylong", "这是一个刻意写得很长的节点标签，用来压标签换行与避让策略"),
]
GRAPH_EDGES = [
    ("a", "b", ""),
    ("b", "c", "通过"),
    ("c", "d", ""),
    ("c", "a", "重试"),      # 环
    ("b", "c", "缓存命中"),   # 与上一条平行的第二条边
    ("wide", "a", ""),
    ("verylong", "d", "长标签出边"),
]

# ── sample_grapheme.txt 的数据 ───────────────────────────────────────────────
# 每行一条用例：<标签>\t<文本>。标签供单测按名定位，文本是待切分的字素簇序列。
# 覆盖：纯 ASCII、单个组合记号、多个组合记号、ZWJ 家庭、区域指示符国旗、
# 泰文（上/下标元音）、梵文簇、谚文、emoji + 变体选择符、孤立组合记号（边界）。
# 组合记号与 ZWJ 用 chr() 拼装：直接写字面量会被编辑器/工具链"规范化"成预组合形式，
# 那样夹具就不再测它想测的东西了。
_C = chr  # 简写


def _cps(*codes: int) -> str:
    return "".join(_C(c) for c in codes)


GRAPHEME_CASES = [
    ("ascii", "abc"),
    ("combining_single", "e" + _cps(0x0301)),
    ("combining_multi", "a" + _cps(0x0306, 0x0301)),
    ("zwj_family", _cps(0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467)),
    ("regional_indicator", _cps(0x1F1E8, 0x1F1F3)),
    ("emoji_vs16", _cps(0x2764, 0xFE0F)),
    ("thai", _cps(0x0E01, 0x0E38)),
    ("devanagari", _cps(0x0915, 0x094D, 0x0937, 0x093F)),
    ("hangul", _cps(0xD55C, 0xAD6D, 0xC5B4)),
    ("lone_combining", _cps(0x0301)),
    ("cjk_wide", _cps(0x4E2D, 0x6587, 0x5BBD, 0x5EA6)),
]


def sample_data_hpp() -> str:
    labels = ", ".join('"%s"' % s for s in LABELS)
    # 注意：外层的一对大括号由模板里的 {{{{ 提供，这里只写元素本身。
    points = ",\n".join(
        "    {%s, %s}" % (fmt(x), fmt(y)) for x, y in SCATTER
    )
    return """// sample_data.hpp —— 由 tools/gen_fixtures.py 生成，禁手改。
//
// 用途：chart/dataset 与各类图种的通用输入样例——折线 / 柱 / 散点共用同一份数据，
// 让"同一组数在不同图种下的表现"可比。数值由生成器算出：x 等距、linear 等差、
// sine = sin(x/2)、counts 为非负计数（柱状语义）、scatter 含负值与重复 x。
//
// 消费方：tests/unit/test_fixtures.cpp（守夹具自身的不变量）。
#ifndef TERMCHART_TESTS_FIXTURES_SAMPLE_DATA_HPP
#define TERMCHART_TESTS_FIXTURES_SAMPLE_DATA_HPP

#include <array>
#include <cstddef>
#include <string_view>

namespace termchart::fixtures {{

// 序列长度：所有 *_Series 的长度都等于它，方便调用方静态断言。
inline constexpr std::size_t kSeriesLength = {n};

// 一条 (x, y) 散点。
struct XyPoint {{
    double x;
    double y;
}};

inline constexpr std::array<double, {n}> kXSeries{{{{{x}}}}};
inline constexpr std::array<double, {n}> kLinearSeries{{{{{linear}}}}};
inline constexpr std::array<double, {n}> kSineSeries{{{{{sine}}}}};
inline constexpr std::array<double, {n}> kCountSeries{{{{{counts}}}}};

inline constexpr std::array<std::string_view, {n}> kCategoryLabels{{{{{labels}}}}};

inline constexpr std::array<XyPoint, {np}> kScatterPoints{{{{
{points}
}}}};

}}  // namespace termchart::fixtures

#endif  // TERMCHART_TESTS_FIXTURES_SAMPLE_DATA_HPP
""".format(
        n=N,
        x=arr(X),
        linear=arr(LINEAR),
        sine=arr(SINE),
        counts=arr(COUNTS),
        labels=labels,
        np=len(SCATTER),
        points=points,
    )


def sample_graph_hpp() -> str:
    # 同样是 % 格式化（不是 .format），所以这里只写一层大括号：外层由模板的 {{{{ 提供。
    nodes = "\n".join('    {"%s", "%s"},' % (i, l) for i, l in GRAPH_NODES)
    edges = "\n".join(
        '    {{"{f}", "{t}", "{l}"}},'.format(f=f, t=t, l=l)
        for f, t, l in GRAPH_EDGES
    )
    return """// sample_graph.hpp —— 由 tools/gen_fixtures.py 生成，禁手改。
//
// 用途：graph 模型的稳定输入样例。刻意覆盖四类边界：
//   * **环**：c → a（布局与拓扑排序必须能处理，不能死循环）；
//   * **孤立点**：iso（度为 0，测遍历 / 删除 / 连通分量）；
//   * **长标签 / 宽字符标签**：压标签换行与"按显示宽度"避让；
//   * **平行边**：b → c 出现两次（标签不同），测去重与并列排布。
//
// 只放**纯数据**（不 include 任何 graph 头）：夹具不该有层依赖，谁都能 include。
// 消费方：tests/unit/test_fixtures.cpp（守夹具自身的不变量）。
#ifndef TERMCHART_TESTS_FIXTURES_SAMPLE_GRAPH_HPP
#define TERMCHART_TESTS_FIXTURES_SAMPLE_GRAPH_HPP

#include <array>
#include <cstddef>
#include <string_view>

namespace termchart::fixtures {{

struct GraphNodeSpec {{
    std::string_view id;
    std::string_view label;
}};

struct GraphEdgeSpec {{
    std::string_view from;
    std::string_view to;
    std::string_view label;
}};

inline constexpr std::array<GraphNodeSpec, {nn}> kGraphNodes{{{{
{nodes}
}}}};

inline constexpr std::array<GraphEdgeSpec, {ne}> kGraphEdges{{{{
{edges}
}}}};

}}  // namespace termchart::fixtures

#endif  // TERMCHART_TESTS_FIXTURES_SAMPLE_GRAPH_HPP
""".format(nn=len(GRAPH_NODES), ne=len(GRAPH_EDGES), nodes=nodes, edges=edges)


def sample_grapheme_txt() -> str:
    lines = [
        "# sample_grapheme.txt —— 由 tools/gen_fixtures.py 生成，禁手改。",
        "#",
        "# 字素簇用例（tests/fixtures/README.md 的规格清单最后一件）。",
        "# 格式：每行一条用例，`<标签>\\t<文本>`；以 # 开头的行是注释，跳过。",
        "# 覆盖：纯 ASCII / 单个组合记号 / 多个组合记号 / ZWJ 家庭 / 区域指示符国旗 /",
        "# emoji+变体选择符 / 泰文上下标元音 / 梵文簇 / 谚文 / 孤立组合记号 / CJK 宽字符。",
        "#",
        "# 注意：本文件含 ZWJ(U+200D) 与组合记号等不可见码点，编辑器的规范化会破坏它，",
        "# 所以它由生成器产出并被 --check 守住——**禁止手改**。",
    ]
    for tag, text in GRAPHEME_CASES:
        lines.append("%s\t%s" % (tag, text))
    return "\n".join(lines) + "\n"


def main(argv: list[str]) -> int:
    check = wants_check(argv)
    changed = emit(FIXTURES / "sample_data.hpp", sample_data_hpp(), check)
    changed |= emit(FIXTURES / "sample_graph.hpp", sample_graph_hpp(), check)
    changed |= emit(FIXTURES / "sample_grapheme.txt", sample_grapheme_txt(), check)
    return 1 if changed and check else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
