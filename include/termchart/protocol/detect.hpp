// detect.hpp —— 从环境变量探测终端支持的图像协议
#ifndef TERMCHART_PROTOCOL_DETECT_HPP
#define TERMCHART_PROTOCOL_DETECT_HPP

#include "termchart/export.hpp"
#include "termchart/protocol/fallback.hpp"

namespace termchart::protocol {

// 读 TERM / TERM_PROGRAM / COLORTERM / KITTY_WINDOW_ID 等推断能力。
// 纯环境读取，不改环境；探测不到一律 false。
TERMCHART_API ProtocolCaps detect_protocol_caps() noexcept;

}  // namespace termchart::protocol

#endif  // TERMCHART_PROTOCOL_DETECT_HPP
