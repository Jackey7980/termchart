// bench_mermaid_parse.cpp —— DSL 解析吞吐基线（规格阶段 5 点名）
//
// 度量 `dsl::parse_*` 一族：解析是"每帧只做一次"的动作，但它决定了
//   * 启动延迟（大图打开前用户要等多久）；
//   * fuzz 的吞吐上限（同一套代码被畸形输入反复调用）。
// 因此按"同一张图用四种方言表达 + 规模翻倍"来测：如果某处方言的复杂度悄悄变成
// 平方级（例如在每行里线性扫描全部已建节点），这里会立刻显形。
//
// 输入全部**现场生成**，不读夹具文件：bench 必须能在任意 cwd 下运行。
#include <cstddef>
#include <string>
#include <vector>

#include "bench.hpp"
#include "termchart/dsl/d2.hpp"
#include "termchart/dsl/graphviz_dot.hpp"
#include "termchart/dsl/mermaid.hpp"
#include "termchart/dsl/plantuml.hpp"

namespace {

using namespace termchart;

std::string mermaid_flowchain(std::size_t nodes) {
    std::string text = "flowchart LR\n";
    for (std::size_t i = 0; i < nodes; ++i) {
        text += "  n" + std::to_string(i);
        if (i + 1 < nodes) {
            text += " -->";
        } else {
            text += "\n";
        }
    }
    return text;
}

std::string dot_flowchain(std::size_t nodes) {
    std::string text = "digraph G {\n rankdir=LR\n";
    for (std::size_t i = 0; i + 1 < nodes; ++i) {
        text += "  n" + std::to_string(i) + " -> n" + std::to_string(i + 1) + "\n";
    }
    text += "}\n";
    return text;
}

std::string plantuml_chain(std::size_t nodes) {
    std::string text = "@startuml\n";
    for (std::size_t i = 0; i < nodes; ++i) {
        text += "class N" + std::to_string(i) + "\n";
    }
    for (std::size_t i = 0; i + 1 < nodes; ++i) {
        text += "N" + std::to_string(i) + " --> N" + std::to_string(i + 1) + "\n";
    }
    text += "@enduml\n";
    return text;
}

std::string d2_chain(std::size_t nodes) {
    std::string text = "direction: right\n";
    for (std::size_t i = 0; i + 1 < nodes; ++i) {
        text += "n" + std::to_string(i) + " -> n" + std::to_string(i + 1) + "\n";
    }
    return text;
}

}  // namespace

int main(int argc, char** argv) {
    const bool quick = bench::is_quick(argc, argv);
    const std::vector<std::size_t> sizes = quick ? std::vector<std::size_t>{200}
                                                 : std::vector<std::size_t>{200, 400, 800, 1600};
    const int rounds = quick ? 3 : 7;

    bench::header("bench_mermaid_parse", "dsl 解析（四种方言，节点链）");

    for (const std::size_t count : sizes) {
        const std::string size_text = std::to_string(count) + " nodes";

        const std::string mermaid = mermaid_flowchain(count);
        bench::report("mermaid/flowchart", size_text,
                      bench::median_ns([&mermaid] { (void)dsl::parse_mermaid(mermaid); }, rounds));

        const std::string dot = dot_flowchain(count);
        bench::report("dot", size_text,
                      bench::median_ns([&dot] { (void)dsl::parse_dot(dot); }, rounds));

        const std::string plantuml = plantuml_chain(count);
        bench::report("plantuml", size_text,
                      bench::median_ns([&plantuml] { (void)dsl::parse_plantuml(plantuml); }, rounds));

        const std::string d2 = d2_chain(count);
        bench::report("d2", size_text,
                      bench::median_ns([&d2] { (void)dsl::parse_d2(d2); }, rounds));
    }
    return 0;
}
