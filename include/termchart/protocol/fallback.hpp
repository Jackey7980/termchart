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
    bool iterm2 = false;
    bool truecolor = false; // 24-bit 颜色，半块降级需要它
};

// 选择顺序：Kitty > Iterm2 > Sixel > HalfBlock（需 truecolor）> None。
// 排在前面的协议保真度更高（像素级原图 + 无需量化），半块只作最后兜底。
TERMCHART_API ProtocolKind pick_protocol(const ProtocolCaps& caps) noexcept;

}  // namespace termchart::protocol

#endif  // TERMCHART_PROTOCOL_FALLBACK_HPP
