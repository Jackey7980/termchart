// fallback.hpp —— 图像协议降级：能力 → ProtocolKind
//
// 与 raster/auto_select 同一套思路：能力以扁平布尔传入，协议层不依赖 term/。
#ifndef TERMCHART_PROTOCOL_FALLBACK_HPP
#define TERMCHART_PROTOCOL_FALLBACK_HPP

#include "termchart/export.hpp"
#include "termchart/protocol/protocol.hpp"

namespace termchart::protocol {

struct ProtocolCaps {
    bool kitty = false;
    bool sixel = false;
    bool iterm2 = false;    // 可被探测到，但编码器依赖 PNG（待补），故不参与选择
    bool truecolor = false; // 24-bit 颜色，半块降级需要它
};

// 选择顺序：Kitty > Sixel > HalfBlock（需 truecolor）> None。
TERMCHART_API ProtocolKind pick_protocol(const ProtocolCaps& caps) noexcept;

}  // namespace termchart::protocol

#endif  // TERMCHART_PROTOCOL_FALLBACK_HPP
