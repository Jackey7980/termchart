// edge.cpp —— Edge 构造
#include "termchart/graph/edge.hpp"

namespace termchart::graph {

Edge::Edge(NodeId from, NodeId to, std::string label)
    : from_(from), to_(to), label_(std::move(label)) {}

}  // namespace termchart::graph
