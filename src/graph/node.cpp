// node.cpp —— Node 构造
#include "termchart/graph/node.hpp"

namespace termchart::graph {

Node::Node(NodeId id, std::string label) : id_(id), label_(std::move(label)) {}

}  // namespace termchart::graph
