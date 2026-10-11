// fallback.cpp —— 协议选择
#include "termchart/protocol/fallback.hpp"

namespace termchart::protocol {

ProtocolKind pick_protocol(const ProtocolCaps& caps) noexcept {
    if (caps.kitty) {
        return ProtocolKind::Kitty;
    }
    if (caps.sixel) {
        return ProtocolKind::Sixel;
    }
    if (caps.truecolor) {
        return ProtocolKind::HalfBlock;
    }
    // 注：caps.iterm2 为真也不选 Iterm2 —— 其编码器需要 PNG 负载（未落地），
    // 选了会给不出可用字节流。宁可退到 HalfBlock / None。
    return ProtocolKind::None;
}

}  // namespace termchart::protocol
