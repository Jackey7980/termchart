// sankey.cpp —— 桑基图渲染：阶段分层 + 流量定高 + 丝带
#include "termchart/graph/diagrams/sankey.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::graph {

namespace {

constexpr char32_t kBarChar = U'\u2588';   // █ 节点横条
constexpr char32_t kFlowChar = U'\u2591';  // ░ 流丝带

}  // namespace

Rect render_sankey(const std::vector<SankeyNode>& nodes,
                   const std::vector<SankeyFlow>& flows, canvas::Canvas& canvas,
                   const SankeyOptions& opts) {
    if (nodes.empty()) {
        return Rect{};
    }
    const std::size_t node_count = nodes.size();

    // 有效流：端点在界内、非自流、value > 0。
    std::vector<double> sum_out(node_count, 0.0);
    std::vector<double> sum_in(node_count, 0.0);
    std::vector<std::size_t> flow_ids;
    flow_ids.reserve(flows.size());
    for (std::size_t f = 0; f < flows.size(); ++f) {
        const SankeyFlow& flow = flows[f];
        if (flow.source >= node_count || flow.target >= node_count ||
            flow.source == flow.target || !(flow.value > 0.0)) {
            continue;
        }
        flow_ids.push_back(f);
        sum_out[flow.source] += flow.value;
        sum_in[flow.target] += flow.value;
    }

    // 阶段深度（最长路径分层）：d[target] = max(d[source] + 1)，源节点 0。
    // 单调增且有界，迭代 node_count + 1 轮必收敛；有环时深度封顶在环长内，
    // 不影响"流恒向右"的不变量（松弛一轮后 d[target] ≥ d[source] + 1 恒成立）。
    std::vector<std::size_t> depth(node_count, 0);
    bool changed = true;
    for (std::size_t iter = 0; changed && iter <= node_count; ++iter) {
        changed = false;
        for (const std::size_t f : flow_ids) {
            const std::size_t s = flows[f].source;
            const std::size_t t = flows[f].target;
            if (depth[t] < depth[s] + 1) {
                depth[t] = depth[s] + 1;
                changed = true;
            }
        }
    }
    std::size_t stage_count = 0;
    for (const std::size_t d : depth) {
        stage_count = std::max(stage_count, d + 1);
    }

    // 节点高 ∝ max(出流合计, 入流合计)，孤节点保底 1 行。
    std::vector<double> unit(node_count, 1.0);
    for (std::size_t i = 0; i < node_count; ++i) {
        unit[i] = std::max(1.0, std::max(sum_out[i], sum_in[i]));
    }
    std::vector<std::vector<std::size_t>> stage(stage_count);
    for (std::size_t i = 0; i < node_count; ++i) {
        stage[depth[i]].push_back(i);
    }
    // 缩放取"最挤阶段"：所有阶段都放得下，各列共用同一比例。
    double max_need = 1.0;
    for (const auto& nodes_in_stage : stage) {
        double need = 0.0;
        for (std::size_t k = 0; k < nodes_in_stage.size(); ++k) {
            need += unit[nodes_in_stage[k]] + (k > 0 ? opts.v_gap : 0.0);
        }
        max_need = std::max(max_need, need);
    }
    const double avail_h =
        std::max(1.0, static_cast<double>(canvas.height()) - 2.0 * opts.margin);
    const double scale = avail_h / max_need;

    // 几何：列 x 逐列累计，列内自上而下堆叠。
    std::vector<double> col_x(stage_count, 0.0);
    col_x[0] = opts.margin;
    for (std::size_t c = 1; c < stage_count; ++c) {
        col_x[c] = col_x[c - 1] + opts.bar_width + opts.h_gap;
    }
    std::vector<double> node_y(node_count, 0.0);
    std::vector<double> node_h(node_count, 0.0);
    for (const auto& nodes_in_stage : stage) {
        double y = opts.margin;
        for (const std::size_t i : nodes_in_stage) {
            node_h[i] = std::max(1.0, std::round(unit[i] * scale));
            node_y[i] = y;
            y += node_h[i] + opts.v_gap * scale;
        }
    }

    // 流厚与槽位：按登记顺序在条上自上而下堆叠；取整可能累计略超条高，夹住。
    std::vector<double> thick(flows.size(), 0.0);
    std::vector<double> slot_out(flows.size(), 0.0);
    std::vector<double> slot_in(flows.size(), 0.0);
    std::vector<double> acc_out(node_count, 0.0);
    std::vector<double> acc_in(node_count, 0.0);
    for (const std::size_t f : flow_ids) {
        const SankeyFlow& flow = flows[f];
        thick[f] = std::max(1.0, std::round(flow.value * scale));
        double so = acc_out[flow.source];
        if (so + thick[f] > node_h[flow.source]) {
            so = std::max(0.0, node_h[flow.source] - thick[f]);
        }
        slot_out[f] = so;
        acc_out[flow.source] = so + thick[f];
        double ti = acc_in[flow.target];
        if (ti + thick[f] > node_h[flow.target]) {
            ti = std::max(0.0, node_h[flow.target] - thick[f]);
        }
        slot_in[f] = ti;
        acc_in[flow.target] = ti + thick[f];
    }

    // 调色板：节点按登记序取 6 色，丝带继承源节点色。
    std::vector<Color> palette(node_count);
    for (std::size_t i = 0; i < node_count; ++i) {
        palette[i] = Color::ansi16(static_cast<std::uint8_t>(1 + i % 6));
    }

    // 1) 丝带（先画；上下沿从源槽位线性过渡到目标槽位）。
    for (const std::size_t f : flow_ids) {
        const SankeyFlow& flow = flows[f];
        const double xs = col_x[depth[flow.source]] + opts.bar_width;
        const double xt = col_x[depth[flow.target]];
        if (xt - xs < 1.0) {
            continue;  // 目标不在右侧（环数据退化）：不画
        }
        const double sy0 = node_y[flow.source] + slot_out[f];
        const double sy1 = sy0 + thick[f];
        const double ty0 = node_y[flow.target] + slot_in[f];
        const double ty1 = ty0 + thick[f];
        Style style = opts.flow_style;
        if (opts.colorize && style.fg().is_none()) {
            style.set_fg(palette[flow.source]);
        }
        const int gx0 = static_cast<int>(std::ceil(xs));
        const int gx1 = static_cast<int>(std::floor(xt));
        for (int gx = gx0; gx < gx1; ++gx) {
            const double t = (static_cast<double>(gx) + 0.5 - xs) / (xt - xs);
            const double ytop = sy0 + (ty0 - sy0) * t;
            const double ybot = sy1 + (ty1 - sy1) * t;
            canvas.fill_rect(
                Rect{static_cast<double>(gx), ytop, 1.0,
                     std::max(0.0, ybot - ytop)},
                kFlowChar, style);
        }
    }

    // 2) 节点横条。
    for (std::size_t i = 0; i < node_count; ++i) {
        Style style = opts.node_style;
        if (opts.colorize && style.fg().is_none()) {
            style.set_fg(palette[i]);
        }
        canvas.fill_rect(Rect{col_x[depth[i]], node_y[i], opts.bar_width, node_h[i]},
                         kBarChar, style);
    }

    // 3) 标签（末阶段画在条左侧，其余画在右侧；最后画，压过丝带）。
    Rect bounds{};
    for (std::size_t i = 0; i < node_count; ++i) {
        bounds = bounds.union_with(
            Rect{col_x[depth[i]], node_y[i], opts.bar_width, node_h[i]});
        const std::string& label = nodes[i].name;
        if (label.empty()) {
            continue;
        }
        const double label_w = static_cast<double>(util::display_width(label));
        const double ly =
            node_y[i] + std::floor(std::max(0.0, node_h[i] - 1.0) / 2.0);
        const double lx = (depth[i] + 1 == stage_count)
                              ? col_x[depth[i]] - label_w
                              : col_x[depth[i]] + opts.bar_width;
        canvas.draw_text(Point{lx, ly}, label, opts.node_style);
        bounds = bounds.union_with(Rect{lx, ly, label_w, 1.0});
    }
    return bounds;
}

}  // namespace termchart::graph
