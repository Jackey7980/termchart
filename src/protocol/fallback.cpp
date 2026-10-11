// fallback.cpp —— 协议选择
#include "termchart/protocol/fallback.hpp"

namespace termchart::protocol {

ProtocolKind pick_protocol(const ProtocolCaps& caps) noexcept {
    if (caps.kitty) {
        return ProtocolKind::Kitty;
    }
    if (caps.iterm2) {
        return ProtocolKind::Iterm2;
    }
    if (caps.sixel) {
        return ProtocolKind::Sixel;
    }
    if (caps.truecolor) {
        return ProtocolKind::HalfBlock;
    }
    return ProtocolKind::None;
}

}  // namespace termchart::protocol
