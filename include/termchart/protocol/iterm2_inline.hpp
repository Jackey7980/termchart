// iterm2_inline.hpp —— iTerm2 内联图像协议（OSC 1337）
//
// iTerm2（及其兼容终端 WezTerm 等）只认 **PNG** 负载：原始 RGBA 不会被识别，
// 因此本编码器建立在内置的 image/png_encode 之上。
//
// 序列形状（BEL 结束）：
//   ESC ] 1337 ; File = inline=1;size=<PNG字节数>;width=<w>px;height=<h>px : <base64> BEL
//
// 与 sixel/kitty 一致，编码结果不含光标定位——放哪儿由调用方决定。
#ifndef TERMCHART_PROTOCOL_ITERM2_INLINE_HPP
#define TERMCHART_PROTOCOL_ITERM2_INLINE_HPP

#include <string>

#include "termchart/export.hpp"
#include "termchart/protocol/protocol.hpp"

namespace termchart::protocol {

class Iterm2Inline final : public Protocol {
public:
    [[nodiscard]] ProtocolKind kind() const noexcept override {
        return ProtocolKind::Iterm2;
    }
    [[nodiscard]] const char* name() const noexcept override { return "iterm2"; }

    // 空位图返回空串；ProtocolOptions 的格尺寸对本协议无意义（宽高就是像素尺寸）。
    [[nodiscard]] std::string encode(const image::Bitmap& bitmap,
                                     const ProtocolOptions& opts = {}) const override;
};

}  // namespace termchart::protocol

#endif  // TERMCHART_PROTOCOL_ITERM2_INLINE_HPP
