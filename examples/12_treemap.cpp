// 12_treemap.cpp —— 树图（规格示例）
//
// 层次数据 → 切片矩形：叶子面积 ∝ 聚合值，内部节点 value<=0 表示"由子节点聚合"
// （HierNode 的约定，aggregate_values 自底向上算）。
#include <cstdint>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/hierarchical/node.hpp"
#include "termchart/chart/hierarchical/treemap.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/size.hpp"

namespace {

using namespace termchart;

chart::hier::HierNode make_tree() {
    chart::hier::HierNode root;
    root.name = "termchart";

    chart::hier::HierNode core;
    core.name = "core";
    core.children.push_back(chart::hier::HierNode{"buffer", 30.0, Color::none(), {}});
    core.children.push_back(chart::hier::HierNode{"style", 18.0, Color::none(), {}});

    chart::hier::HierNode charts;
    charts.name = "chart";
    charts.children.push_back(chart::hier::HierNode{"cartesian", 40.0, Color::none(), {}});
    charts.children.push_back(chart::hier::HierNode{"polar", 22.0, Color::none(), {}});

    chart::hier::HierNode graph;
    graph.name = "graph";
    graph.children.push_back(chart::hier::HierNode{"layout", 26.0, Color::none(), {}});
    graph.children.push_back(chart::hier::HierNode{"route", 14.0, Color::none(), {}});

    root.children.push_back(core);
    root.children.push_back(charts);
    root.children.push_back(graph);
    return root;
}

}  // namespace

int main() {
    chart::hier::HierNode root = make_tree();
    chart::hier::assign_colors(root);

    chart::Treemap chart;
    chart.set_title("loc by module");
    chart.set_root(root);

    const term::TerminalSize size = term::query_size();
    const int rows = size.rows == 0 ? 24 : size.rows;
    canvas::Canvas canvas(56, static_cast<std::uint32_t>(rows > 6 ? rows - 2 : 14));
    chart.draw({canvas});

    render::Renderer renderer;
    (void)api::write_stdout(renderer.render_full(canvas.to_buffer()));
    (void)api::write_stdout("\n");
    (void)api::flush_stdout();
    return 0;
}
