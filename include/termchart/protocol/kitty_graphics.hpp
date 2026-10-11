// kitty_graphics.hpp —— Kitty 图形协议（ESC_G，原始 RGBA/RGB + base64 分块）
#ifndef TERMCHART_PROTOCOL_KITTY_GRAPHICS_HPP
#define TERMCHART_PROTOCOL_KITTY_GRAPHICS_HPP

#include "termchart/protocol/protocol.hpp"

namespace termchart::protocol {

class KittyProtocol final : public Protocol {
public:
    [[nodiscard]] ProtocolKind kind() const noexcept override { return ProtocolKind::Kitty; }
    [[nodiscard]] const char* name() const noexcept override { return "kitty"; }
    [[nodiscard]] std::string encode(const image::Bitmap& bitmap,
                                    const ProtocolOptions& opts = {}) const override;
};

}  // namespace termchart::protocol

#endif  // TERMCHART_PROTOCOL_KITTY_GRAPHICS_HPP
