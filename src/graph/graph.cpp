// graph.cpp —— 图容器实现
#include "termchart/graph/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace termchart::graph {

Node& Graph::add_node(std::string label) {
    nodes_.emplace_back(next_id_++, std::move(label));
    return nodes_.back();
}

Node* Graph::find(NodeId id) noexcept {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                                 [id](const Node& n) { return n.id() == id; });
    return it == nodes_.end() ? nullptr : &(*it);
}

const Node* Graph::find(NodeId id) const noexcept {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                                 [id](const Node& n) { return n.id() == id; });
    return it == nodes_.end() ? nullptr : &(*it);
}

bool Graph::remove_node(NodeId id) {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                                 [id](const Node& n) { return n.id() == id; });
    if (it == nodes_.end()) {
        return false;
    }
    nodes_.erase(it);
    // 连带删除所有引用该节点的边。
    edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
                                [id](const Edge& e) {
                                    return e.from() == id || e.to() == id;
                                }),
                 edges_.end());
    return true;
}

Edge* Graph::add_edge(NodeId from, NodeId to, std::string label) {
    if (find(from) == nullptr || find(to) == nullptr) {
        return nullptr;
    }
    edges_.emplace_back(from, to, std::move(label));
    return &edges_.back();
}

Edge* Graph::find_edge(NodeId from, NodeId to) noexcept {
    for (Edge& edge : edges_) {
        if (edge.from() == from && edge.to() == to) {
            return &edge;
        }
    }
    return nullptr;
}

const Edge* Graph::find_edge(NodeId from, NodeId to) const noexcept {
    for (const Edge& edge : edges_) {
        if (edge.from() == from && edge.to() == to) {
            return &edge;
        }
    }
    return nullptr;
}

bool Graph::remove_edge(NodeId from, NodeId to) {
    const auto it = std::find_if(edges_.begin(), edges_.end(),
                                 [from, to](const Edge& e) {
                                     return e.from() == from && e.to() == to;
                                 });
    if (it == edges_.end()) {
        return false;
    }
    edges_.erase(it);
    return true;
}

std::vector<const Edge*> Graph::edges_of(NodeId id) const {
    std::vector<const Edge*> out;
    for (const Edge& e : edges_) {
        if (e.from() == id || e.to() == id) {
            out.push_back(&e);
        }
    }
    return out;
}

std::size_t Graph::out_degree(NodeId id) const noexcept {
    std::size_t count = 0;
    for (const Edge& e : edges_) {
        if (e.from() == id) {
            ++count;
        }
    }
    return count;
}

std::size_t Graph::in_degree(NodeId id) const noexcept {
    std::size_t count = 0;
    for (const Edge& e : edges_) {
        if (e.to() == id) {
            ++count;
        }
    }
    return count;
}

Rect Graph::bounds() const noexcept {
    if (nodes_.empty()) {
        return Rect{};
    }
    double min_x = nodes_.front().box().x;
    double min_y = nodes_.front().box().y;
    double max_x = nodes_.front().box().right();
    double max_y = nodes_.front().box().bottom();
    for (const Node& n : nodes_) {
        min_x = std::min(min_x, n.box().x);
        min_y = std::min(min_y, n.box().y);
        max_x = std::max(max_x, n.box().right());
        max_y = std::max(max_y, n.box().bottom());
    }
    return Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

void Graph::clear() {
    nodes_.clear();
    edges_.clear();
    next_id_ = 1;
}

}  // namespace termchart::graph
