// protocol.cpp —— Protocol 基类虚表 + 名字
#include "termchart/protocol/protocol.hpp"

namespace termchart::protocol {

Protocol::~Protocol() = default;

const char* protocol_name(ProtocolKind kind) noexcept {
    switch (kind) {
        case ProtocolKind::None:
            return "none";
        case ProtocolKind::Sixel:
            return "sixel";
        case ProtocolKind::Kitty:
            return "kitty";
        case ProtocolKind::HalfBlock:
            return "halfblock";
    }
    return "unknown";
}

}  // namespace termchart::protocol
