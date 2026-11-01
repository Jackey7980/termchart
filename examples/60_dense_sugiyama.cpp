// 60_dense_sugiyama.cpp —— 密集图上的 Sugiyama：分层 → 降交叉 → 坐标分配 → 正交路由
//
// 与 56（10 节点的小图）对照，本例专门把规模压到"必然出现大量交叉"的量级：
//   默认 7 层 × 10 节点 = 70 节点，相邻层逆序扇出 + 跨 2 层长边 + 1 条回边（环）。
//   用法：60_dense_sugiyama [每层节点数] [--full]
//         --full 不把画布钳到终端可视区（图比终端大时默认钳制，避免折行错位）。
//
// 布局四步仍全部走库实现，示例内不手写布局算法：
//   ① 分层      layout_sugiyama 内部：先回边反转破环，再最长路径定层
//   ② 降交叉    layout_sugiyama 内部：median 双向扫描
//   ③ 坐标分配  layout_sugiyama 内部：层 = 列，层内按降交叉后的顺序纵向排列
//   ④ 边路由    graph::route_edges(RouteStyle::Orthogonal) + WireGrid 避障
//
// 绘制与 56 的关键差异：连线不再是"后画的覆盖先画的"。每条正交段只把方向位
// （上/下/左/右）累积到格上，最后统一用 graph::junction_char 合成字符，因此
// 交叉处会真的出现 ┼ / ├ / ┬，而不是被最后一条边盖掉。斜段是路由退化（正交
// 绕不通）的产物，方块字符表达不了，单独画并覆盖。
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/image/png_encode.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/arrow.hpp"
#include "termchart/graph/crossing.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/layout/sugiyama.hpp"
#include "termchart/graph/port.hpp"
#include "termchart/graph/route/junction.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/shape/shape.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/terminal.hpp"
#include "termchart/util/random.hpp"
#include "termchart/util/wcwidth.hpp"

namespace {

using namespace termchart;

constexpr std::uint32_t kLayerCount = 7;
constexpr std::uint32_t kDefaultPerLayer = 10;

// 边的三种来源，只是为了在密集图里把它们画成不同颜色，好区分。
enum class EdgeKind : unsigned char {
    Adjacent = 0,  // 相邻层
    Long = 1,      // 跨 2 层的长边（分层后可能跨更多层）
    Back = 2,      // 回边（制造环）
};

Style style_for(EdgeKind kind) noexcept {
    switch (kind) {
        case EdgeKind::Long:
            return Style{}.set_fg(Color::ansi16(11));  // 黄：跨层长边
        case EdgeKind::Back:
            return Style{}.set_fg(Color::ansi16(9));   // 红：回边
        case EdgeKind::Adjacent:
        default:
            return Style{}.set_fg(Color::ansi16(8));   // 灰：层间主边
    }
}

struct DenseGraph {
    graph::Graph graph;
    std::vector<EdgeKind> kinds;  // 与 graph.edges() 一一对应（只登记成功的边）
};

// 生成：层内节点按 A0..G9 命名；相邻层按"镜像 + 抖动"连边，天然逆序 → 交叉密集。
// with_cycle 打开时补一条"最后一层 → 第 0 层"的回边：破环会把它翻过来参与分层，
// 代价是这条边在图上仍是 G→A 的方向（逆着 rank 的长边），正好用来压 regression。
// 关掉"回边反转"的那个版本在这里会让整条链塌到第 0 层（L0 塞进 17 个节点）。
DenseGraph build_dense_graph(std::uint32_t per_layer, util::Rng& rng, bool with_cycle,
                             bool tall_labels) {
    graph::Graph g;
    std::vector<std::vector<graph::NodeId>> ids(kLayerCount);
    for (std::uint32_t l = 0; l < kLayerCount; ++l) {
        ids[l].reserve(per_layer);
        for (std::uint32_t k = 0; k < per_layer; ++k) {
            std::string label(1, static_cast<char>('A' + static_cast<int>(l)));
            label += std::to_string(k);
            if (tall_labels && (k + l) % 3u == 0u) {
                label += "\n+";  // 两行标签 ⇒ 框高 +1。注意偏移量带 l：若每层
                                 // 用同一规则，各层的高度模式就完全相同，"共享
                                 // 格点网"根本没被破坏——那等于没做不等高实验。
            }
            ids[l].push_back(g.add_node(label).id());
        }
    }

    std::vector<EdgeKind> kinds;
    auto link = [&](graph::NodeId from, graph::NodeId to, EdgeKind kind) {
        if (g.add_edge(from, to) != nullptr) {  // 重复边会被拒绝，此时不登记 kind
            kinds.push_back(kind);
        }
    };

    for (std::uint32_t l = 0; l + 1 < kLayerCount; ++l) {
        // 先保证下层每个节点都有入边：来源是镜像位置，天然逆序 → 交叉密集。
        // （漏掉这一步会有一批下层节点入度为 0，被最长路径分层判到 rank 0，
        //   整张图的层就散了。）
        for (std::uint32_t k = 0; k < per_layer; ++k) {
            link(ids[l][per_layer - 1u - k], ids[l + 1][k], EdgeKind::Adjacent);
        }
        // 再随机补一批（镜像 + 抖动），把边密度顶上去。
        for (std::uint32_t k = 0; k < per_layer; ++k) {
            if (rng.integer(2) == 0) {
                continue;
            }
            const std::uint32_t mirror = per_layer - 1u - k;
            const std::uint32_t target =
                (mirror + static_cast<std::uint32_t>(rng.integer(3))) % per_layer;
            link(ids[l][k], ids[l + 1][target], EdgeKind::Adjacent);
        }
    }
    for (std::uint32_t l = 0; l + 2 < kLayerCount; ++l) {
        for (std::uint32_t t = 0; t < 2; ++t) {
            link(ids[l][static_cast<std::size_t>(rng.integer(per_layer))],
                 ids[l + 2][static_cast<std::size_t>(rng.integer(per_layer))], EdgeKind::Long);
        }
    }
    if (with_cycle) {
        link(ids[kLayerCount - 1][0], ids[0][0], EdgeKind::Back);
    }

    return DenseGraph{std::move(g), std::move(kinds)};
}

// 交叉数：节点中心连线段两两判定（graph::count_crossings，O(E^2) 精确计数）。
std::size_t count_center_crossings(const graph::Graph& graph) {
    std::vector<std::pair<Point, Point>> segments;
    segments.reserve(graph.edge_count());
    for (const graph::Edge& edge : graph.edges()) {
        const graph::Node* from = graph.find(edge.from());
        const graph::Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        segments.emplace_back(from->box().center(), to->box().center());
    }
    return graph::count_crossings(segments);
}

std::size_t count_by_kind(const std::vector<EdgeKind>& kinds, EdgeKind kind) noexcept {
    return static_cast<std::size_t>(
        std::count(kinds.begin(), kinds.end(), kind));
}

// ── 绘制 ────────────────────────────────────────────────────────────────
// 正交段按方向位累积到格上（位序与 junction_char 一致：上 1 / 下 2 / 左 4 / 右 8），
// 走线总长度（曼哈顿格数）：Σ 每段 |dx| + |dy|。降交叉只盯交叉数会奖励"把长边
// 甩到很远的行位"，所以这个数必须能打印出来，否则改进只看肉眼。
double total_route_length(const graph::Graph& graph) {
    double sum = 0.0;
    for (const graph::Edge& edge : graph.edges()) {
        const std::vector<Point>& path = edge.route();
        for (std::size_t i = 1; i < path.size(); ++i) {
            sum += std::abs(path[i].x - path[i - 1].x) + std::abs(path[i].y - path[i - 1].y);
        }
    }
    return sum;
}

// 节点高度分布：验证"不等高"是否真的生效（若全是一档，说明场景没建立起来）。
std::string height_histogram(const graph::Graph& graph, const graph::LayoutOptions& opts) {
    std::map<int, int> hist;
    for (const graph::Node& node : graph.nodes()) {
        hist[static_cast<int>(std::lround(node_box_for(node, opts).h))] += 1;
    }
    std::string out;
    for (const auto& entry : hist) {
        out += "h" + std::to_string(entry.first) + "×" + std::to_string(entry.second) + "  ";
    }
    return out;
}

// 平均拐角数：每条边的拐角 = 折线点数 - 2（直线 0、标准 Z 形 2）。
double bend_ratio(const graph::Graph& graph) {
    std::size_t bends = 0;
    for (const graph::Edge& edge : graph.edges()) {
        if (edge.route().size() >= 2) {
            bends += edge.route().size() - 2;
        }
    }
    return static_cast<double>(bends) /
           static_cast<double>(std::max<std::size_t>(graph.edge_count(), 1));
}

// 平均对齐误差：每条边两端中心行位的平均绝对差。
double align_error(const graph::Graph& graph) {
    double sum = 0.0;
    for (const graph::Edge& edge : graph.edges()) {
        const graph::Node* from = graph.find(edge.from());
        const graph::Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        sum += std::abs((from->box().y + from->box().h / 2.0) -
                        (to->box().y + to->box().h / 2.0));
    }
    return sum / static_cast<double>(std::max<std::size_t>(graph.edge_count(), 1));
}

// 两端中心行位**完全相同**（能一条水平线走完）的边占比。这是"边直不直"最直接的
// 度量：平均对齐误差小并不等于很多边真的直（每边都差一点，路由照样每条拐两次）。
double aligned_ratio(const graph::Graph& graph) {
    std::size_t aligned = 0;
    std::size_t total = 0;
    for (const graph::Edge& edge : graph.edges()) {
        const graph::Node* from = graph.find(edge.from());
        const graph::Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        ++total;
        if (std::lround(from->box().y + from->box().h / 2.0) ==
            std::lround(to->box().y + to->box().h / 2.0)) {
            ++aligned;
        }
    }
    return total == 0 ? 0.0 : static_cast<double>(aligned) / static_cast<double>(total);
}

// 拐点数：折线里方向改变的次数。层内 y 对齐做得好时，更多边可以一条直线走完，
// 这个数比"总长度"更贴近"看起来直不直"。
std::size_t total_turns(const graph::Graph& graph) {
    std::size_t turns = 0;
    for (const graph::Edge& edge : graph.edges()) {
        const std::vector<Point>& path = edge.route();
        for (std::size_t i = 2; i < path.size(); ++i) {
            const bool prev_h = std::lround(path[i - 1].y) == std::lround(path[i - 2].y);
            const bool cur_h = std::lround(path[i].y) == std::lround(path[i - 1].y);
            if (prev_h != cur_h) {
                ++turns;
            }
        }
    }
    return turns;
}

// 长边骨架的"上下折返"次数：沿路径取水平段的 y 序列，方向反转即记一次。
// 只作观测：两条长边互相穿插时 dummy 合法地需要折返，所以这不是硬断言。
std::size_t long_edge_y_reversals(const graph::Graph& graph) {
    std::size_t bad = 0;
    for (const graph::Edge& edge : graph.edges()) {
        const std::vector<Point>& path = edge.route();
        if (path.size() < 4) {
            continue;  // 非长边骨架
        }
        std::vector<double> rows;
        for (std::size_t i = 1; i < path.size(); ++i) {
            if (std::lround(path[i].y) == std::lround(path[i - 1].y)) {
                rows.push_back(path[i].y);
            }
        }
        int dir = 0;
        for (std::size_t i = 1; i < rows.size(); ++i) {
            const double delta = rows[i] - rows[i - 1];
            if (std::abs(delta) < 1.0e-9) {
                continue;
            }
            const int sign = delta > 0.0 ? 1 : -1;
            if (dir == 0) {
                dir = sign;
            } else if (sign != dir) {
                ++bad;
                break;
            }
        }
    }
    return bad;
}

// 长边穿过中间列实节点框的格数（沿折线逐格采样；端点框不算）。dummy 生效后
// 应为 0——长边横穿中间列时走的是 dummy 的槽位行，那一行没有实节点框。
std::size_t count_through_boxes(const graph::Graph& graph) {
    std::size_t hits = 0;
    for (const graph::Edge& edge : graph.edges()) {
        const graph::Node* from = graph.find(edge.from());
        const graph::Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const std::vector<Point>& path = edge.route();
        bool hit = false;
        for (std::size_t i = 1; i < path.size() && !hit; ++i) {
            const long ax = std::lround(path[i - 1].x);
            const long ay = std::lround(path[i - 1].y);
            const long bx = std::lround(path[i].x);
            const long by = std::lround(path[i].y);
            const long steps = std::max(std::labs(bx - ax), std::labs(by - ay));
            for (long s = 1; s < steps && !hit; ++s) {
                const long x = ax + (bx - ax) * s / steps;
                const long y = ay + (by - ay) * s / steps;
                for (const graph::Node& n : graph.nodes()) {
                    if (&n == from || &n == to || n.box().w <= 0.0) {
                        continue;
                    }
                    const Rect& b = n.box();
                    if (static_cast<double>(x) > b.x && static_cast<double>(x) < b.right() &&
                        static_cast<double>(y) > b.y && static_cast<double>(y) < b.bottom()) {
                        hit = true;
                        break;
                    }
                }
            }
        }
        if (hit) {
            ++hits;  // 一条边只记一次
        }
    }
    return hits;
}

// 交汇口由 junction_char 合成；冲突时颜色取最后写入的那条边。
void draw_edges(canvas::Canvas& canvas, const graph::Graph& graph,
                const std::vector<EdgeKind>& kinds, std::size_t& diagonal_segments) {
    const auto w = static_cast<int>(canvas.width());
    const auto h = static_cast<int>(canvas.height());
    std::vector<unsigned char> dir(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);
    std::vector<unsigned char> kind_at(dir.size(), 0);

    for (std::size_t k = 0; k < graph.edges().size(); ++k) {
        const std::vector<Point>& path = graph.edges()[k].route();
        const EdgeKind kind = k < kinds.size() ? kinds[k] : EdgeKind::Adjacent;
        const Style style = style_for(kind);
        for (std::size_t i = 1; i < path.size(); ++i) {
            const int ax = static_cast<int>(std::lround(path[i - 1].x));
            const int ay = static_cast<int>(std::lround(path[i - 1].y));
            const int bx = static_cast<int>(std::lround(path[i].x));
            const int by = static_cast<int>(std::lround(path[i].y));
            if (ay == by) {  // 水平段
                const int y = ay;
                const int x0 = std::min(ax, bx);
                const int x1 = std::max(ax, bx);
                for (int x = x0; x <= x1; ++x) {
                    if (x < 0 || x >= w || y < 0 || y >= h) {
                        continue;
                    }
                    const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                            static_cast<std::size_t>(x);
                    if (x > x0) {
                        dir[idx] |= 4u;  // 有左侧来线
                    }
                    if (x < x1) {
                        dir[idx] |= 8u;  // 有右侧来线
                    }
                    kind_at[idx] = static_cast<unsigned char>(kind);
                }
            } else if (ax == bx) {  // 垂直段
                const int x = ax;
                const int y0 = std::min(ay, by);
                const int y1 = std::max(ay, by);
                for (int y = y0; y <= y1; ++y) {
                    if (x < 0 || x >= w || y < 0 || y >= h) {
                        continue;
                    }
                    const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                            static_cast<std::size_t>(x);
                    if (y > y0) {
                        dir[idx] |= 1u;  // 有上方来线
                    }
                    if (y < y1) {
                        dir[idx] |= 2u;  // 有下方来线
                    }
                    kind_at[idx] = static_cast<unsigned char>(kind);
                }
            } else {  // 正交绕不通，退化为斜线：方块字符表达不了，直接覆盖
                ++diagonal_segments;
                const bool down_right = (by > ay) == (bx > ax);
                canvas.draw_line(path[i - 1], path[i], down_right ? U'\u2572' : U'\u2571', style);
            }
        }
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t idx =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
            const unsigned char bits = dir[idx];
            if (bits == 0u) {
                continue;
            }
            const char32_t cp =
                graph::junction_char((bits & 1u) != 0u, (bits & 2u) != 0u, (bits & 4u) != 0u,
                                     (bits & 8u) != 0u);
            if (cp == U' ') {
                continue;
            }
            canvas.put(Point{static_cast<double>(x), static_cast<double>(y)}, cp,
                       style_for(static_cast<EdgeKind>(kind_at[idx])));
        }
    }
}

void draw_nodes(canvas::Canvas& canvas, const graph::Graph& graph, const Style& style) {
    for (const graph::Node& node : graph.nodes()) {
        graph::draw_shape(canvas, graph::ShapeKind::Box, node.box(), style);
        if (node.label().empty()) {
            continue;
        }
        // 多行标签逐行居中（'--tall' 对照用两行标签制造"不等高节点"）。
        const std::string_view label = node.label();
        const int line_count = static_cast<int>(std::count(label.begin(), label.end(), '\n')) + 1;
        const double first_y =
            node.box().y + std::floor(std::max(0.0, node.box().h - static_cast<double>(line_count)) /
                                      2.0);
        int line = 0;
        std::size_t start = 0;
        while (true) {
            const std::size_t nl = label.find('\n', start);
            const std::string_view text = nl == std::string_view::npos
                                              ? label.substr(start)
                                              : label.substr(start, nl - start);
            const int width = util::display_width(text);
            const double x =
                node.box().x + std::max(0.0, (node.box().w - static_cast<double>(width)) / 2.0);
            canvas.draw_text(Point{x, first_y + line}, text, style);
            ++line;
            if (nl == std::string_view::npos) {
                break;
            }
            start = nl + 1;
        }
    }
}

void draw_arrows(canvas::Canvas& canvas, const graph::Graph& graph) {
    for (const graph::Edge& edge : graph.edges()) {
        const std::vector<Point>& path = edge.route();
        if (path.size() < 2 || edge.arrow() == graph::ArrowStyle::None) {
            continue;
        }
        const Style style = style_for(EdgeKind::Adjacent);
        canvas.put(path.back(),
                   graph::arrow_head_char(graph::side_towards(path[path.size() - 2], path.back())),
                   style);
    }
}

// 箭头头部：位图/矢量出口此前**完全不画箭头** ⇒ 方向信息全丢（分层图没了方向就
// 没意义：读者分不清哪边是上游）。终端路径有 draw_arrows()，位图路径没有——
// 同一份数据、两个出口给出两种语义，这是真缺口。
//
// 画法：沿最后一段方向在末端画一个"V"形箭头（两条短线），tip 落在 route 末端
// （终点节点框的锚点）。场景单位 = 1 格，箭头长度取 1.2 格。
void draw_arrowhead(scene::SceneBuilder& b, Point tip, Point prev, Color color) {
    const double dx = tip.x - prev.x;
    const double dy = tip.y - prev.y;
    const double len = std::hypot(dx, dy);
    if (!(len > 1.0e-9)) {
        return;
    }
    const double ux = dx / len;
    const double uy = dy / len;
    constexpr double kHalfAngle = 0.42;  // ≈24°
    constexpr double kSize = 1.2;        // 箭头长度（场景单位）
    for (const double sign : {1.0, -1.0}) {
        const double ca = std::cos(sign * kHalfAngle);
        const double sa = std::sin(sign * kHalfAngle);
        const double rx = ux * ca - uy * sa;
        const double ry = ux * sa + uy * ca;
        b.stroke(color, 1.0);
        b.move_to(tip);
        b.line_to(tip.x - rx * kSize, tip.y - ry * kSize);
    }
}

// 把布局与路由结果（节点框 + 折线 + 标签）装成 Scene：密集图在 1 格 1 字符的
// 画布上只能靠 ┼ ╱ 表达，看不清真形状，需要矢量/位图才能看清。
scene::Scene build_scene(const graph::Graph& graph, const std::vector<EdgeKind>& kinds) {
    scene::Scene scene;
    scene::SceneBuilder b(scene);

    for (std::size_t k = 0; k < graph.edges().size(); ++k) {
        const std::vector<Point>& route = graph.edges()[k].route();
        if (route.size() < 2) {
            continue;
        }
        const EdgeKind kind = k < kinds.size() ? kinds[k] : EdgeKind::Adjacent;
        const Color color = style_for(kind).fg();
        b.stroke(color, 1.0);
        b.move_to(route.front());
        for (std::size_t i = 1; i < route.size(); ++i) {
            b.line_to(route[i]);
        }
        // 箭头（终点）：此前位图/矢量出口不画，方向信息全丢。
        const graph::Edge& edge = graph.edges()[k];
        if (edge.arrow() == graph::ArrowStyle::AtTarget || edge.arrow() == graph::ArrowStyle::Both) {
            draw_arrowhead(b, route.back(), route[route.size() - 2], color);
        }
        if (edge.arrow() == graph::ArrowStyle::Both) {
            draw_arrowhead(b, route.front(), route[1], color);
        }
    }
    for (const graph::Node& node : graph.nodes()) {
        b.stroke(Color::rgb(86, 197, 154), 1.0);
        b.rect(node.box());
        if (!node.label().empty()) {
            b.text(Point{node.box().x + 1.0, node.box().y + 1.5}, node.label(),
                   Style{}.set_fg(Color::rgb(86, 197, 154)));
        }
    }

    return scene;
}

bool write_svg(const scene::Scene& scene, const std::string& path) {
    backend::vector::SvgBackend backend;
    // 场景单位 = 1 格，字号就得是 1 格；不设的话后端按 scale 当字号，文字会
    // 放大 scale 倍糊满整张图。
    backend.set_options(backend::vector::VectorOptions{.scale = 14.0, .font_size = 1.0});
    const std::string svg = backend::vector::render_vector(backend, scene);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << svg;
    return static_cast<bool>(out);
}

// 位图导出：Scene → gui::SceneRenderer → PNG（与 55 同一条链路）。
bool write_png(const scene::Scene& scene, const std::string& path) {
    gui::RenderOptions options;
    // 画布按场景自身比例给：autofit 是等比的（min(avail_w/b.w, avail_h/b.h)，
    // 单倍率），比例不对只会在两侧留黑边，不会拉伸——但留白难看。
    const Rect b = scene.bounds();
    const double aspect = (b.w > 0.0 && b.h > 0.0) ? b.w / b.h : 16.0 / 9.0;
    options.width = 1600;
    options.height = static_cast<std::uint32_t>(1600.0 / aspect);
    options.autofit = true;
    options.supersample = 2;
    // text_cell 是 **Scene 坐标**（随 autofit 缩放），场景单位 = 1 格 ⇒ 填 1.0。
    // 填 16 会被当成"每格 16 场景单位"，一个字放大到几百像素糊满整张图。
    options.text_cell_w = 1.0;
    options.text_cell_h = 1.0;
    gui::SceneRenderer renderer(options);
    const image::Bitmap bitmap = renderer.render(scene);
    const std::string png = image::encode_png(bitmap);
    if (png.empty()) {
        return false;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(png.data(), static_cast<std::streamsize>(png.size()));
    return static_cast<bool>(out);
}

// 布局后按 x 分组还原真实分层（x 才是层，nodes() 的遍历顺序是登记顺序）。
std::string layer_report(const graph::Graph& graph) {
    std::map<double, std::vector<std::pair<double, std::string>>> by_x;
    for (const graph::Node& node : graph.nodes()) {
        by_x[node.box().x].emplace_back(node.box().y, node.label());
    }
    std::string out;
    std::size_t layer = 0;
    for (auto& entry : by_x) {
        std::sort(entry.second.begin(), entry.second.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        out += "  L" + std::to_string(layer++) + " (" + std::to_string(entry.second.size()) + "):";
        for (const auto& placed : entry.second) {
            out += " ";
            out += placed.second;
        }
        out += "\n";
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    std::uint32_t per_layer = kDefaultPerLayer;
    bool full = false;
    bool with_cycle = false;
    bool with_align = false;
    bool with_tall = false;
    std::string svg_path;
    std::string png_path;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] == nullptr ? std::string{} : std::string{argv[i]};
        if (arg == "--full") {
            full = true;
            continue;
        }
        if (arg == "--cycle") {
            with_cycle = true;
            continue;
        }
        if (arg == "--align") {
            with_align = true;
            continue;
        }
        if (arg == "--tall") {
            with_tall = true;
            continue;
        }
        if (arg == "--svg" && i + 1 < argc) {
            svg_path = argv[++i];
            continue;
        }
        if (arg == "--png" && i + 1 < argc) {
            png_path = argv[++i];
            continue;
        }
        const int parsed = std::atoi(arg.c_str());
        if (parsed > 0) {
            per_layer = static_cast<std::uint32_t>(std::min(parsed, 40));
        }
    }

    term::Terminal terminal(term::TerminalOptions{.alt_screen = false, .hide_cursor = false});
    if (terminal.enter() != ErrorCode::Ok) {
        std::fprintf(stderr, "无法进入终端模式\n");
        return 1;
    }

    util::Rng rng(0x5EED1234ull);  // 固定种子：每次跑出来的图完全一致
    DenseGraph built = build_dense_graph(per_layer, rng, with_cycle, with_tall);
    graph::Graph& graph = built.graph;

    // 密集图要塞进终端：框压到 3 行、层内不留垂直间隙。
    // h_gap 必须留够：层间要容得下 Z 形绕行的竖段，太窄会让正交路由找不到通路、
    // 退化成斜线（实测 h_gap=4 时 103 条边里有 55 段退化，6 时降到个位数）。
    graph::LayoutOptions layout;
    layout.min_width = 5.0;
    layout.min_height = 3.0;
    layout.h_gap = 8.0;
    layout.v_gap = 1.0;  // 框贴框会把每层连成一列"墙"，留 1 行呼吸感
    layout.margin = 1.0;
    // 层内坐标分配：默认紧堆（实测全指标最优）；--align 打开朴素邻居对齐作对照
    layout.align_rows = with_align;

    // 基线：只分层、层内按登记顺序（不做降交叉）。
    (void)graph::layout_layered(graph, layout);
    const std::size_t crossings_before = count_center_crossings(graph);

    // 收敛曲线：每次都从登记顺序重排，故各轮独立可比。
    const std::uint32_t sweep_table[] = {1u, 2u, 3u, 4u, 6u, 8u, 12u};
    std::string convergence;
    for (const std::uint32_t sweeps : sweep_table) {
        (void)graph::layout_sugiyama(graph, layout, sweeps);
        convergence += "  " + std::to_string(sweeps) + " 轮 → " +
                       std::to_string(count_center_crossings(graph)) + " 交叉\n";
    }

    // 最终布局（8 轮）。
    const Rect bounds = graph::layout_sugiyama(graph, layout, 8);
    const std::size_t crossings_after = count_center_crossings(graph);

    const term::TerminalSize screen = terminal.size();
    std::uint32_t canvas_w = static_cast<std::uint32_t>(std::ceil(bounds.right())) + 2u;
    std::uint32_t canvas_h = static_cast<std::uint32_t>(std::ceil(bounds.bottom())) + 2u;
    bool clipped = false;
    if (!full && screen.valid()) {
        const std::uint32_t max_w = static_cast<std::uint32_t>(screen.cols);
        const std::uint32_t max_h =
            static_cast<std::uint32_t>(screen.rows > 1 ? screen.rows - 1 : screen.rows);
        clipped = canvas_w > max_w || canvas_h > max_h;
        canvas_w = std::min(canvas_w, max_w);
        canvas_h = std::min(canvas_h, max_h);
    }
    canvas::Canvas canvas(std::max<std::uint32_t>(canvas_w, 8u),
                          std::max<std::uint32_t>(canvas_h, 4u));

    graph::WireGrid grid(canvas.width(), canvas.height());
    graph::RouteOptions route_opts;
    route_opts.max_detour = 8;
    // 密集图里连线相交是必然的：不让连线互避（只避节点框），否则先到的边把层间
    // 通道占满后，后来的边找不到任何正交通路、退化成斜线。相交处由 junction_char
    // 合成 ┼ 表达。稀疏图（56）保持默认 avoid_wires = true 更清爽。
    route_opts.avoid_wires = false;
    // 布局阶段已给跨多层长边写好沿 dummy 槽位的折线，别再让通用 Z 形覆盖它。
    route_opts.keep_existing_routes = true;
    (void)graph::route_edges(graph, graph::RouteStyle::Orthogonal, route_opts, &grid);

    if (!svg_path.empty() || !png_path.empty()) {
        const scene::Scene scene = build_scene(graph, built.kinds);
        if (!svg_path.empty() && !write_svg(scene, svg_path)) {
            std::fprintf(stderr, "无法写 SVG：%s\n", svg_path.c_str());
            return 1;
        }
        if (!png_path.empty() && !write_png(scene, png_path)) {
            std::fprintf(stderr, "无法写 PNG：%s\n", png_path.c_str());
            return 1;
        }
        std::fprintf(stderr, "已写出：%s %s\n", svg_path.c_str(), png_path.c_str());
    }

    const std::size_t through_boxes = count_through_boxes(graph);
    const long total_length = std::lround(total_route_length(graph));
    const std::size_t y_reversals = long_edge_y_reversals(graph);
    const std::size_t turn_count = total_turns(graph);
    char bend_buf[32];
    std::snprintf(bend_buf, sizeof(bend_buf), "%.2f", bend_ratio(graph));
    char align_buf[32];
    std::snprintf(align_buf, sizeof(align_buf), "%.2f", align_error(graph));
    char straight_buf[32];
    std::snprintf(straight_buf, sizeof(straight_buf), "%.2f", aligned_ratio(graph));

    std::size_t diagonal_segments = 0;
    draw_edges(canvas, graph, built.kinds, diagonal_segments);
    draw_nodes(canvas, graph, Style{}.set_fg(Color::rgb(86, 197, 154)));
    draw_arrows(canvas, graph);

    render::Renderer renderer(terminal.caps().color_depth);
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    if (api::write_stdout(bytes) != ErrorCode::Ok) {
        return 1;
    }

    std::string summary = "\n密集 Sugiyama：" + std::to_string(graph.node_count()) + " 节点 / " +
                          std::to_string(graph.edge_count()) + " 边（层间 " +
                          std::to_string(count_by_kind(built.kinds, EdgeKind::Adjacent)) +
                          "、跨层 " + std::to_string(count_by_kind(built.kinds, EdgeKind::Long)) +
                          "、回边 " + std::to_string(count_by_kind(built.kinds, EdgeKind::Back)) +
                          "）\n" + "交叉数（节点中心连线段，O(E^2) 精确计数）：" +
                          std::to_string(crossings_before) + " → " +
                          std::to_string(crossings_after) + "\nmedian 扫描轮数收敛：\n" +
                          convergence + "正交路由退化成斜线的段数：" +
                          std::to_string(diagonal_segments) + "\n长边穿过中间列节点框的条数：" +
                          std::to_string(through_boxes) + "\n走线总长度（曼哈顿格）：" +
                          std::to_string(total_length) + "\n折线拐点总数：" +
                          std::to_string(turn_count) + "\n平均拐角数（bend_ratio）：" + bend_buf +
                          "\n平均对齐误差（align_error）：" + align_buf +
                          "\n能一条直线走完的边占比：" + straight_buf +
                          "\n长边骨架上下折返次数：" +
                          std::to_string(y_reversals) + "\n节点高度分布：" +
                          height_histogram(graph, layout) + "\n" + "层内顺序（布局后）：\n" +
                          layer_report(graph);
    if (clipped) {
        summary += "\n（图比终端可视区大，已裁剪显示；用 --full 输出完整图）\n";
    }
    if (with_cycle) {
        summary += "（--cycle：含回边。破环时它被反转参与分层，"
                   "但绘制方向仍是 G→A，故是一条逆行长边）\n";
    }
    (void)api::write_stdout(summary);
    (void)api::flush_stdout();
    return terminal.leave() == ErrorCode::Ok ? 0 : 1;
}
