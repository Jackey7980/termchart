// halfblock_image.hpp —— 真彩半块字符图像（任何 24-bit 终端可用的降级）
//
// 每格塞 2 个像素：上像素当前景、下像素当背景，字符恒为 ▀（U+2580）。
// 于是 1 格 = 1×2 像素，观感接近位图，且不需要任何图形协议。
#ifndef TERMCHART_PROTOCOL_HALFBLOCK_IMAGE_HPP
#define TERMCHART_PROTOCOL_HALFBLOCK_IMAGE_HPP

#include "termchart/protocol/protocol.hpp"

namespace termchart::protocol {

class HalfBlockProtocol final : public Protocol {
public:
    [[nodiscard]] ProtocolKind kind() const noexcept override { return ProtocolKind::HalfBlock; }
    [[nodiscard]] const char* name() const noexcept override { return "halfblock"; }
    [[nodiscard]] std::string encode(const image::Bitmap& bitmap,
                                    const ProtocolOptions& opts = {}) const override;
};

}  // namespace termchart::protocol

#endif  // TERMCHART_PROTOCOL_HALFBLOCK_IMAGE_HPP
