// force.cpp —— 力导向布局实现
#include "termchart/graph/layout/force.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace termchart::graph {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

Rect union_of_boxes(const Graph& graph) noexcept {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    double min_x = graph.nodes().front().box().x;
    double min_y = graph.nodes().front().box().y;
    double max_x = graph.nodes().front().box().right();
    double max_y = graph.nodes().front().box().bottom();
    for (const Node& n : graph.nodes()) {
        min_x = std::min(min_x, n.box().x);
        min_y = std::min(min_y, n.box().y);
        max_x = std::max(max_x, n.box().right());
        max_y = std::max(max_y, n.box().bottom());
    }
    return Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

}  // namespace

Rect layout_force(Graph& graph, const ForceOptions& opts) {
    const std::size_t count = graph.nodes().size();
    if (count == 0) {
        return Rect{};
    }

    // 节点尺寸先定下来，斥力按"中心点"计算，最后再落格。
    std::vector<Rect> size(count);
    for (std::size_t i = 0; i < count; ++i) {
        size[i] = node_box_for(graph.nodes()[i], opts.layout);
    }

    const double k = std::max(1.0, opts.ideal_edge_length);
    // 初始圆环半径：面积随节点数增长，保证初始不过度重叠。
    const double radius = std::max(k, k * std::sqrt(static_cast<double>(count)) * 0.6);
    std::vector<double> px(count);
    std::vector<double> py(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double angle = kTwoPi * static_cast<double>(i) / static_cast<double>(count);
        px[i] = radius * std::cos(angle);
        py[i] = radius * std::sin(angle);
    }

    std::vector<double> dx(count, 0.0);
    std::vector<double> dy(count, 0.0);
    double temperature = radius * 0.5;

    for (std::uint32_t iter = 0; iter < opts.iterations && temperature > opts.min_step;
         ++iter) {
        std::fill(dx.begin(), dx.end(), 0.0);
        std::fill(dy.begin(), dy.end(), 0.0);

        // 斥力：所有节点两两。
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t j = i + 1; j < count; ++j) {
                double vx = px[i] - px[j];
                double vy = py[i] - py[j];
                double dist = std::sqrt(vx * vx + vy * vy);
                if (dist < 1e-6) {
                    // 完全重合：给一个固定方向的微小分离，保持确定性。
                    vx = 1e-3;
                    vy = 0.0;
                    dist = 1e-3;
                }
                const double force = (k * k) / dist;
                const double ux = vx / dist;
                const double uy = vy / dist;
                dx[i] += ux * force;
                dy[i] += uy * force;
                dx[j] -= ux * force;
                dy[j] -= uy * force;
            }
        }

        // 引力：沿边。
        for (const Edge& edge : graph.edges()) {
            const Node* from = graph.find(edge.from());
            const Node* to = graph.find(edge.to());
            if (from == nullptr || to == nullptr) {
                continue;
            }
            const auto i = static_cast<std::size_t>(from - graph.nodes().data());
            const auto j = static_cast<std::size_t>(to - graph.nodes().data());
            if (i == j) {
                continue;
            }
            const double vx = px[i] - px[j];
            const double vy = py[i] - py[j];
            const double dist = std::max(1e-6, std::sqrt(vx * vx + vy * vy));
            const double force = (dist * dist) / k;
            const double ux = vx / dist;
            const double uy = vy / dist;
            dx[i] -= ux * force;
            dy[i] -= uy * force;
            dx[j] += ux * force;
            dy[j] += uy * force;
        }

        // 弱引力：把所有节点朝原点 (0,0) 轻轻收拢。连通图因边引力已自居中，此项
        // 近乎无感；但它防止"无连边分量"（纯斥力、无引力）在冷却冻结前漂成离散
        // 外壳。纯线性弹簧到原点 + 确定性（无随机），不破坏 golden/回归。
        for (std::size_t i = 0; i < count; ++i) {
            dx[i] -= opts.gravity * px[i];
            dy[i] -= opts.gravity * py[i];
        }

        // 限步长降温后位移。
        for (std::size_t i = 0; i < count; ++i) {
            const double len = std::sqrt(dx[i] * dx[i] + dy[i] * dy[i]);
            if (len < 1e-9) {
                continue;
            }
            const double step = std::min(len, temperature);
            px[i] += dx[i] / len * step;
            py[i] += dy[i] / len * step;
        }
        temperature *= opts.cooling;
    }

    // 硬碰撞移除：落格前做几轮 O(n^2) 分离，把重叠的节点对沿最小穿透轴推开。
    // 斥力通常已分离，但标签长短不一时仍可能贴住；此趟保证不重叠（ASCII 栅格下
    // 重叠 = 不可读）。确定性顺序（i,j 升序），不改变已有不重叠图的几何。
    {
        const double slack = 1.0;  // 额外留白，避免"刚好挨着"被判重叠
        for (int relax = 0; relax < 8; ++relax) {
            bool moved = false;
            for (std::size_t i = 0; i < count; ++i) {
                for (std::size_t j = i + 1; j < count; ++j) {
                    const double aw = size[i].w + slack, ah = size[i].h + slack;
                    const double bw = size[j].w + slack, bh = size[j].h + slack;
                    const double dcx = (px[j] - size[j].w / 2.0) - (px[i] - size[i].w / 2.0);
                    const double dcy = (py[j] - size[j].h / 2.0) - (py[i] - size[i].h / 2.0);
                    const double ox = (aw + bw) / 2.0 - std::fabs(dcx);  // X 穿透量
                    const double oy = (ah + bh) / 2.0 - std::fabs(dcy);  // Y 穿透量
                    if (ox <= 0.0 || oy <= 0.0) {
                        continue;  // 本轴不重叠
                    }
                    if (ox < oy) {
                        const double s = (dcx >= 0.0 ? 1.0 : -1.0) * ox / 2.0;
                        px[i] -= s;
                        px[j] += s;
                    } else {
                        const double s = (dcy >= 0.0 ? 1.0 : -1.0) * oy / 2.0;
                        py[i] -= s;
                        py[j] += s;
                    }
                    moved = true;
                }
            }
            if (!moved) {
                break;
            }
        }
    }

    // 落格：位置是中心点，先按中心算外框左上角，再整体平移到边距并取整。
    double min_x = px[0] - size[0].w / 2.0;
    double min_y = py[0] - size[0].h / 2.0;
    for (std::size_t i = 0; i < count; ++i) {
        min_x = std::min(min_x, px[i] - size[i].w / 2.0);
        min_y = std::min(min_y, py[i] - size[i].h / 2.0);
    }
    for (std::size_t i = 0; i < count; ++i) {
        const double x = std::round(px[i] - size[i].w / 2.0 - min_x) + opts.layout.margin;
        const double y = std::round(py[i] - size[i].h / 2.0 - min_y) + opts.layout.margin;
        graph.nodes()[i].set_box(Rect{x, y, size[i].w, size[i].h});
    }
    return union_of_boxes(graph);
}

}  // namespace termchart::graph
