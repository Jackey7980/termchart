// protocol.hpp —— 终端图形协议抽象基类
//
// protocol/ 只负责一件事：把 image::Bitmap **编码成可写入终端的字节流**。
// 它不做布局、不移动光标、不写 stdout——定位由调用方决定。于是同一张位图可以
// 用 sixel / kitty / 半块字符三种方式表达，降级链见 fallback.hpp。
//
// 注：iTerm2 内联图像需要 PNG 负载（原始 RGBA 不被识别），PNG 编码器（image/png_encode）
// 尚未落地，故暂不提供 Iterm2 编码器——宁缺毋滥。
#ifndef TERMCHART_PROTOCOL_PROTOCOL_HPP
#define TERMCHART_PROTOCOL_PROTOCOL_HPP

#include <cstdint>
#include <string>

#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"

namespace termchart::protocol {

enum class ProtocolKind {
    None,       // 不支持任何图像协议（调用方应转而画字符）
    Sixel,      // DEC sixel（DCS 0;0;0q ... ST）
    Kitty,      // Kitty 图形协议（ESC_G，原始 RGBA）
    HalfBlock,  // 真彩半块字符（▀ + 前景/背景色），任何 24-bit 终端可用
};

struct ProtocolOptions {
    std::uint32_t cell_width_px = 10;   // 目标格的像素宽（kitty 缩放用）
    std::uint32_t cell_height_px = 20;  // 目标格的像素高
    bool rle = true;                    // sixel 游程压缩
};

class Protocol {
public:
    virtual ~Protocol();

    [[nodiscard]] virtual ProtocolKind kind() const noexcept = 0;
    [[nodiscard]] virtual const char* name() const noexcept = 0;

    // 编码为字节流（不含光标定位）。空位图返回空串。
    [[nodiscard]] virtual std::string encode(const image::Bitmap& bitmap,
                                            const ProtocolOptions& opts = {}) const = 0;
};

TERMCHART_API const char* protocol_name(ProtocolKind kind) noexcept;

}  // namespace termchart::protocol

#endif  // TERMCHART_PROTOCOL_PROTOCOL_HPP
