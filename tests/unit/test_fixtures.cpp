// test_fixtures.cpp —— 夹具的"自身不变量"测试
//
// 夹具是生成物（tools/gen_fixtures.py），但**生成物也要有消费方**：没人 include 的
// 夹具等于死文件，改了生成器也不会有人发现。
//
// 本文件守的是夹具自己的契约（而不是库的行为）：
//   * sample_data：各序列长度一致、x 严格递增、计数非负——任一条破了，所有用它的
//     chart 用例都会在同一处"莫名"变红；
//   * sample_graph：节点 id 唯一、边的两端必须存在、确实含环与孤立点
//     （这两条是这份夹具存在的全部理由）；
//   * sample_grapheme：每条用例都能被切分，且若干用例的簇数是确定的。
#include <doctest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "termchart/util/grapheme.hpp"

#include "tests/fixtures/sample_data.hpp"
#include "tests/fixtures/sample_graph.hpp"

#ifdef TERMCHART_FIXTURES_DIR

namespace {

namespace fx = termchart::fixtures;

std::string read_fixture(const char* name) {
    // 与 tests/unit/dsl/test_dsl_fixtures.cpp 同一套写法（std::filesystem::path 拼接）：
    // 仓库路径含非 ASCII 字符，直接拼 std::string 在 MinGW 上会踩窄字符转换。
    const std::filesystem::path path = std::filesystem::path(TERMCHART_FIXTURES_DIR) / name;
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        std::cerr << "读不到夹具：" << path.string() << "\n";
    }
    REQUIRE(input.good());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// 码点数（簇数的上界）：切分永远不会把簇切得比码点还多。
std::size_t codepoint_count(std::string_view text) {
    std::size_t n = 0;
    for (const char ch : text) {
        // 先转 unsigned char 再比较：char 的符号性依平台而定，直接位运算会被
        // -Wsign-conversion 拦下，且负值右移/比较会出错。
        const auto byte = static_cast<unsigned char>(ch);
        // UTF-8 续字节形如 10xxxxxx
        if ((byte & 0xC0u) != 0x80u) {
            ++n;
        }
    }
    return n;
}

}  // namespace

TEST_CASE("fixtures: sample_data 的结构不变量") {
    static_assert(fx::kXSeries.size() == fx::kSeriesLength);
    static_assert(fx::kLinearSeries.size() == fx::kSeriesLength);
    static_assert(fx::kSineSeries.size() == fx::kSeriesLength);
    static_assert(fx::kCountSeries.size() == fx::kSeriesLength);
    static_assert(fx::kCategoryLabels.size() == fx::kSeriesLength);

    // x 严格递增：折线的定义域，重复或回退会让"单调递增"类断言失去意义。
    for (std::size_t i = 1; i < fx::kXSeries.size(); ++i) {
        CHECK(fx::kXSeries[i - 1] < fx::kXSeries[i]);
    }

    // linear = 2x + 1（夹具的语义约定，写死在这里，生成器改了数值就会红）
    for (std::size_t i = 0; i < fx::kSeriesLength; ++i) {
        CHECK(fx::kLinearSeries[i] == 2.0 * fx::kXSeries[i] + 1.0);
    }

    // counts 是非负计数：柱状图拿到负值应是调用方的错，不该由夹具带进去
    for (const double v : fx::kCountSeries) {
        CHECK(v >= 0.0);
    }

    for (const double v : fx::kSineSeries) {
        CHECK((v >= -1.0 && v <= 1.0));
    }

    REQUIRE(fx::kScatterPoints.size() == 8);
    const bool has_negative_x = std::any_of(
        fx::kScatterPoints.begin(), fx::kScatterPoints.end(),
        [](const fx::XyPoint& p) { return p.x < 0.0; });
    CHECK(has_negative_x);

    for (const std::string_view label : fx::kCategoryLabels) {
        CHECK_FALSE(label.empty());
    }
}

TEST_CASE("fixtures: sample_graph 的结构不变量") {
    // 1) 节点 id 唯一
    std::unordered_set<std::string_view> ids;
    for (const auto& n : fx::kGraphNodes) {
        CHECK_FALSE(n.id.empty());
        CHECK(ids.insert(n.id).second);
    }

    // 2) 每条边的两端都必须指向存在的节点（否则构图代码会静默丢边）
    for (const auto& e : fx::kGraphEdges) {
        CHECK(ids.count(e.from) == 1);
        CHECK(ids.count(e.to) == 1);
    }

    // 3) 确实含环：这份夹具就是为"环"准备的。用迭代式 DFS 三色标记检测，
    //    而不是硬编码 c→a——生成器换了拓扑也要继续成立。
    std::unordered_map<std::string_view, std::vector<std::string_view>> adj;
    for (const auto& e : fx::kGraphEdges) {
        adj[e.from].push_back(e.to);
    }
    enum class Color { White, Gray, Black };
    std::unordered_map<std::string_view, Color> color;
    for (const auto& n : fx::kGraphNodes) {
        color[n.id] = Color::White;
    }
    bool has_cycle = false;
    for (const auto& n : fx::kGraphNodes) {
        if (color[n.id] != Color::White) {
            continue;
        }
        std::vector<std::pair<std::string_view, std::size_t>> stack;
        stack.emplace_back(n.id, std::size_t{0});
        color[n.id] = Color::Gray;
        while (!stack.empty()) {
            auto& top = stack.back();
            const std::string_view node = top.first;
            auto it = adj.find(node);
            if (it == adj.end() || top.second >= it->second.size()) {
                color[node] = Color::Black;
                stack.pop_back();
                continue;
            }
            const std::string_view next = it->second[top.second++];
            if (color[next] == Color::Gray) {
                has_cycle = true;
                break;
            }
            if (color[next] == Color::White) {
                color[next] = Color::Gray;
                stack.emplace_back(next, std::size_t{0});
            }
        }
        if (has_cycle) {
            break;
        }
    }
    CHECK(has_cycle);

    // 4) 确实含孤立点（度为 0）
    std::unordered_map<std::string_view, int> degree;
    for (const auto& e : fx::kGraphEdges) {
        ++degree[e.from];
        ++degree[e.to];
    }
    const bool has_isolated = std::any_of(
        fx::kGraphNodes.begin(), fx::kGraphNodes.end(),
        [&](const fx::GraphNodeSpec& n) { return degree[n.id] == 0; });
    CHECK(has_isolated);

    // 5) 确实含长标签（否则"标签换行 / 避让"这条覆盖点形同虚设）
    const bool has_long_label = std::any_of(
        fx::kGraphNodes.begin(), fx::kGraphNodes.end(),
        [](const fx::GraphNodeSpec& n) { return n.label.size() >= 20; });
    CHECK(has_long_label);
}

TEST_CASE("fixtures: sample_grapheme 用例可被切分") {
    const std::string contents = read_fixture("sample_grapheme.txt");
    REQUIRE_FALSE(contents.empty());

    std::istringstream in(contents);
    std::string line;
    std::size_t cases = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;  // 注释
        }
        const auto tab = line.find('\t');
        REQUIRE(tab != std::string::npos);  // 格式：标签 \t 文本
        const std::string tag = line.substr(0, tab);
        const std::string_view value(line);
        const std::string_view text = value.substr(tab + 1);
        REQUIRE_FALSE(text.empty());

        // 任何用例都至少切出一个簇，且簇数不超过码点数这个上界。
        const std::size_t clusters = termchart::util::cluster_count(text);
        CHECK(clusters >= 1);
        CHECK(clusters <= codepoint_count(text));

        // 全量分割与计数必须一致（split_clusters 是热路径之外那条路，别让它悄悄分叉）
        CHECK(termchart::util::split_clusters(text).size() == clusters);

        // 确定性用例：这些值由 UAX #29 直接钉死，改了就是回归。
        if (tag == "ascii") {
            CHECK(clusters == 3);
        } else if (tag == "combining_single" || tag == "combining_multi") {
            CHECK(clusters == 1);  // GB9：×Extend 不切
        } else if (tag == "zwj_family") {
            CHECK(clusters == 1);  // GB11：ZWJ × 扩展象形
        } else if (tag == "regional_indicator") {
            CHECK(clusters == 1);  // GB12/GB13：地区旗标成对
        } else if (tag == "emoji_vs16") {
            CHECK(clusters == 1);  // 变体选择符是 Extend
        } else if (tag == "hangul") {
            CHECK(clusters == 3);  // 三个完整音节，各自成簇
        } else if (tag == "cjk_wide") {
            CHECK(clusters == 4);
        } else if (tag == "lone_combining") {
            CHECK(clusters == 1);  // GB1：文本开头，孤立组合记号自成簇
        }
        // thai / devanagari 依赖 SpacingMark(GB9a) 与 virama 处理，只守上界不钉死数值。

        ++cases;
    }
    CHECK(cases >= 10);  // 夹具被裁短了要能发现
}

#else
#error "TERMCHART_FIXTURES_DIR 未定义：夹具路径由 CMake 注入，缺了它用例会被静默丢弃"
#endif
