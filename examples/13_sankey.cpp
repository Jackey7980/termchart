// 13_sankey.cpp —— 桑基图（规格示例）
//
// "访问 -> 加购 -> 下单 -> 支付" 四段转化漏斗：节点高 ∝ 流量合计，
// 流画成 ░ 丝带，各阶段按可达深度分列；每段流失汇入同一"流失"节点。
// 数据是确定性的；输出自包含（固定画布，不依赖终端尺寸）。
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/sankey.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    std::vector<graph::SankeyNode> nodes;
    nodes.push_back({"access"});
    nodes.push_back({"cart"});
    nodes.push_back({"order"});
    nodes.push_back({"pay"});
    nodes.push_back({"lost"});

    std::vector<graph::SankeyFlow> flows;
    flows.push_back({0, 1, 4000.0});  // access  -> cart
    flows.push_back({0, 4, 6000.0});  // access  -> lost
    flows.push_back({1, 2, 2500.0});  // cart    -> order
    flows.push_back({1, 4, 1500.0});  // cart    -> lost
    flows.push_back({2, 3, 1800.0});  // order   -> pay
    flows.push_back({2, 4, 700.0});   // order   -> lost

    canvas::Canvas canvas(40, 20);
    graph::SankeyOptions opts;
    opts.h_gap = 9.0;  // 末列标签画在条左侧，间距要留出标签宽
    (void)graph::render_sankey(nodes, flows, canvas, opts);

    render::Renderer renderer;
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    const Status status = api::write_stdout(bytes);
    (void)api::write_stdout("\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
