// sixel.hpp —— DEC sixel 编码器（DCS 0;0;0q … ST）
#ifndef TERMCHART_PROTOCOL_SIXEL_HPP
#define TERMCHART_PROTOCOL_SIXEL_HPP

#include "termchart/protocol/protocol.hpp"

namespace termchart::protocol {

class SixelProtocol final : public Protocol {
public:
    [[nodiscard]] ProtocolKind kind() const noexcept override { return ProtocolKind::Sixel; }
    [[nodiscard]] const char* name() const noexcept override { return "sixel"; }
    [[nodiscard]] std::string encode(const image::Bitmap& bitmap,
                                    const ProtocolOptions& opts = {}) const override;
};

}  // namespace termchart::protocol

#endif  // TERMCHART_PROTOCOL_SIXEL_HPP
