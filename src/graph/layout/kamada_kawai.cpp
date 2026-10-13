// kamada_kawai.cpp —— KK 全局能量布局实现
#include "termchart/graph/layout/kamada_kawai.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace termchart::graph {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kMinSeparation = 1e-6;   // 节点中心的最小间距（防止除零/重合）
constexpr int kMaxLocalSteps = 100;       // 单个节点对 Newton 内循环上限
constexpr double kGradientEps = 1e-12;    // 局部梯度低于此值视为已收敛
constexpr double kDeterminantEps = 1e-12; // Newton 二阶矩阵奇异阈值

// 无向邻接表：端点缺失或自环的边不参与布局。
std::vector<std::vector<std::size_t>> undirected_adjacency(const Graph& graph) {
    const std::size_t count = graph.nodes().size();
    std::vector<std::vector<std::size_t>> adj(count);
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
        adj[i].push_back(j);
        adj[j].push_back(i);
    }
    return adj;
}

// 连通分量（孤立节点自成分量）；分量内节点按登记顺序，保证确定性。
std::vector<std::vector<std::size_t>> connected_components(
    const std::vector<std::vector<std::size_t>>& adj) {
    std::vector<int> comp(adj.size(), -1);
    std::vector<std::vector<std::size_t>> components;
    for (std::size_t s = 0; s < adj.size(); ++s) {
        if (comp[s] >= 0) {
            continue;
        }
        const int id = static_cast<int>(components.size());
        components.emplace_back();
        std::vector<std::size_t> queue{s};
        comp[s] = id;
        for (std::size_t head = 0; head < queue.size(); ++head) {
            components.back().push_back(queue[head]);
            for (const std::size_t nb : adj[queue[head]]) {
                if (comp[nb] < 0) {
                    comp[nb] = id;
                    queue.push_back(nb);
                }
            }
        }
    }
    return components;
}

// 单连通分量的 KK 布局：members 为全图节点下标，结果写入 centers（原点在
// 0,0，由调用方统一平移落格）。调用方保证 members 非空且互相连通。
void layout_component(const std::vector<std::vector<std::size_t>>& adj,
                      const std::vector<std::size_t>& members, double edge_length,
                      std::uint32_t max_iterations, double tolerance,
                      std::vector<double>& centers_x, std::vector<double>& centers_y) {
    const std::size_t n = members.size();
    if (n == 1) {
        centers_x[members[0]] = 0.0;
        centers_y[members[0]] = 0.0;
        return;
    }
    if (n == 2) {
        // 两点间必有路径：直接按目标距离水平对放，不需要迭代。
        centers_x[members[0]] = 0.0;
        centers_y[members[0]] = 0.0;
        centers_x[members[1]] = edge_length;
        centers_y[members[1]] = 0.0;
        return;
    }

    // 全图下标 → 成员下标。
    std::vector<std::size_t> local_of(centers_x.size(), n);
    for (std::size_t i = 0; i < n; ++i) {
        local_of[members[i]] = i;
    }

    // 全对最短跳数（成员间 BFS；-1 = 不可达，分量内不会出现）。
    std::vector<double> hop(n * n, -1.0);
    for (std::size_t s = 0; s < n; ++s) {
        std::vector<std::size_t> queue{members[s]};
        std::vector<std::size_t> depth{0};  // 与 queue 平行：depth[i] = 第 i 个入队节点的跳数。
        hop[s * n + s] = 0.0;
        for (std::size_t head = 0; head < queue.size(); ++head) {
            const std::size_t cur = queue[head];
            for (const std::size_t nb : adj[cur]) {
                const std::size_t local = local_of[nb];
                if (local >= n || hop[s * n + local] >= 0.0) {
                    continue;
                }
                hop[s * n + local] = static_cast<double>(depth[head] + 1);
                depth.push_back(depth[head] + 1);
                queue.push_back(nb);
            }
        }
    }

    // 初始圆环：相邻弦长恰好等于目标边长（r = L / (2 sin(π/n))），残差从 0 量级
    // 起步。半径过大（如 r ∝ n·L）会让对径距离远超目标，局部 Newton 把节点
    // 拉向"不可达目标"的中点，三角形会塌缩成 4 格间距的退化解。
    const double radius =
        edge_length / (2.0 * std::sin(kTwoPi / (2.0 * static_cast<double>(n))));
    std::vector<double> px(n);
    std::vector<double> py(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double angle = kTwoPi * static_cast<double>(i) / static_cast<double>(n);
        px[i] = radius * std::cos(angle);
        py[i] = radius * std::sin(angle);
    }

    const double max_step = edge_length * static_cast<double>(n);
    for (std::uint32_t iter = 0; iter < max_iterations; ++iter) {
        // 找能量残差最大的节点对：残差 = |实际距离 - 目标距离| / 目标距离。
        std::size_t best_i = n;
        double best_delta = tolerance;
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                const double target = edge_length * hop[i * n + j];
                const double dx = px[i] - px[j];
                const double dy = py[i] - py[j];
                const double d = std::sqrt(dx * dx + dy * dy);
                if (d < kMinSeparation) {
                    continue;
                }
                const double delta = std::fabs(d - target) / target;
                if (delta > best_delta) {
                    best_delta = delta;
                    best_i = i;
                }
            }
        }
        if (best_i == n) {
            break;  // 全部残差低于 tolerance，已收敛。
        }

        // 对 best_i 做 Newton 迭代（其余节点固定），解局部能量极小。
        for (int step = 0; step < kMaxLocalSteps; ++step) {
            double ex = 0.0;
            double ey = 0.0;
            double dxx = 0.0;
            double dyy = 0.0;
            double dxy = 0.0;
            bool degenerate = false;
            for (std::size_t m = 0; m < n && !degenerate; ++m) {
                if (m == best_i) {
                    continue;
                }
                const double dx = px[best_i] - px[m];
                const double dy = py[best_i] - py[m];
                const double d = std::sqrt(dx * dx + dy * dy);
                if (d < kMinSeparation) {
                    degenerate = true;  // 与其它节点重合：本轮不动，等下一对处理。
                    break;
                }
                const double target = edge_length * hop[best_i * n + m];
                const double k = 1.0 / (target * target);
                const double ux = dx / d;
                const double uy = dy / d;
                ex += k * (d - target) * ux;
                ey += k * (d - target) * uy;
                dxx += k * (1.0 - (target / d) * (1.0 - ux * ux));
                dyy += k * (1.0 - (target / d) * (1.0 - uy * uy));
                dxy += k * target * ux * uy / d;
            }
            if (degenerate ||
                (std::fabs(ex) < kGradientEps && std::fabs(ey) < kGradientEps)) {
                break;
            }
            const double det = dxx * dyy - dxy * dxy;
            if (!std::isfinite(det) || std::fabs(det) < kDeterminantEps) {
                break;  // 二阶矩阵奇异：跳过该步，换下一对。
            }
            double sx = (-ex * dyy + dxy * ey) / det;
            double sy = (-dxx * ey + dxy * ex) / det;
            const double len = std::sqrt(sx * sx + sy * sy);
            if (len > max_step) {
                sx *= max_step / len;
                sy *= max_step / len;
            }
            const double nx = px[best_i] + sx;
            const double ny = py[best_i] + sy;
            bool collide = false;
            for (std::size_t m = 0; m < n && !collide; ++m) {
                if (m == best_i) {
                    continue;
                }
                const double dx = nx - px[m];
                const double dy = ny - py[m];
                collide = dx * dx + dy * dy < kMinSeparation * kMinSeparation;
            }
            if (collide) {
                break;
            }
            px[best_i] = nx;
            py[best_i] = ny;
        }
    }

    for (std::size_t i = 0; i < n; ++i) {
        centers_x[members[i]] = px[i];
        centers_y[members[i]] = py[i];
    }
}

}  // namespace

Rect layout_kamada_kawai(Graph& graph, const KamadaKawaiOptions& opts) {
    const std::size_t count = graph.nodes().size();
    if (count == 0) {
        return Rect{};
    }

    const double edge_length = std::max(1.0, opts.edge_length);
    const std::vector<std::vector<std::size_t>> adj = undirected_adjacency(graph);
    const std::vector<std::vector<std::size_t>> components = connected_components(adj);

    // 节点尺寸先定下来，能量按"中心点"计算，最后再落格。
    std::vector<Rect> size(count);
    for (std::size_t i = 0; i < count; ++i) {
        size[i] = node_box_for(graph.nodes()[i], opts.layout);
    }

    std::vector<double> centers_x(count, 0.0);
    std::vector<double> centers_y(count, 0.0);
    // 非连通图：分量间无弹簧约束（最短路径距离不存在），各自独立布局后从左到
    // 右并排摆放；分量顺序按首个节点的登记顺序，保证确定性。
    double cursor_x = 0.0;
    for (const std::vector<std::size_t>& members : components) {
        layout_component(adj, members, edge_length, opts.max_iterations, opts.tolerance,
                         centers_x, centers_y);

        double min_x = centers_x[members[0]] - size[members[0]].w / 2.0;
        double min_y = centers_y[members[0]] - size[members[0]].h / 2.0;
        double max_x = centers_x[members[0]] + size[members[0]].w / 2.0;
        for (const std::size_t idx : members) {
            min_x = std::min(min_x, centers_x[idx] - size[idx].w / 2.0);
            min_y = std::min(min_y, centers_y[idx] - size[idx].h / 2.0);
            max_x = std::max(max_x, centers_x[idx] + size[idx].w / 2.0);
        }
        const double off_x = cursor_x + opts.layout.margin - min_x;
        const double off_y = opts.layout.margin - min_y;
        for (const std::size_t idx : members) {
            const double x = std::round(centers_x[idx] - size[idx].w / 2.0 + off_x);
            const double y = std::round(centers_y[idx] - size[idx].h / 2.0 + off_y);
            graph.nodes()[idx].set_box(Rect{x, y, size[idx].w, size[idx].h});
        }
        cursor_x += (max_x - min_x) + std::max(1.0, opts.layout.h_gap);
    }
    return graph.bounds();
}

}  // namespace termchart::graph
