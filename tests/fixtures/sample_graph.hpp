// sample_graph.hpp —— 由 tools/gen_fixtures.py 生成，禁手改。
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

namespace termchart::fixtures {

struct GraphNodeSpec {
    std::string_view id;
    std::string_view label;
};

struct GraphEdgeSpec {
    std::string_view from;
    std::string_view to;
    std::string_view label;
};

inline constexpr std::array<GraphNodeSpec, 7> kGraphNodes{{
    {"a", "读取输入"},
    {"b", "解析"},
    {"c", "校验"},
    {"d", "写出结果"},
    {"iso", "孤立节点（度为 0）"},
    {"wide", "宽字符标签"},
    {"verylong", "这是一个刻意写得很长的节点标签，用来压标签换行与避让策略"},
}};

inline constexpr std::array<GraphEdgeSpec, 7> kGraphEdges{{
    {"a", "b", ""},
    {"b", "c", "通过"},
    {"c", "d", ""},
    {"c", "a", "重试"},
    {"b", "c", "缓存命中"},
    {"wide", "a", ""},
    {"verylong", "d", "长标签出边"},
}};

}  // namespace termchart::fixtures

#endif  // TERMCHART_TESTS_FIXTURES_SAMPLE_GRAPH_HPP
