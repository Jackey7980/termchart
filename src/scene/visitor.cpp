#include "termchart/scene/visitor.hpp"

#include <variant>

namespace termchart::scene {

CommandVisitor::~CommandVisitor() = default;

void accept(const Command& command, CommandVisitor& visitor) {
    std::visit([&visitor](const auto& alternative) { visitor.visit(alternative); }, command);
}

void accept_all(const Scene& scene, CommandVisitor& visitor) {
    for (const Command& command : scene.commands()) {
        accept(command, visitor);
    }
}

}  // namespace termchart::scene
