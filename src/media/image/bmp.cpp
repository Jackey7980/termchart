// bmp.cpp —— BMP 解码实现（自主，零依赖）
//
// BMP 是小端格式（与库内其它大端解析器正好相反），全部按小端读。
// 每像素 → RGBA：调色板路径查表；24bpp 直接 BGR→RGB；高位深按掩码抽取。
#include "termchart/media/image/bmp.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace termchart::image {
namespace {

std::uint16_t rd_u16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[0]) |
                                      (static_cast<std::uint16_t>(p[1]) << 8));
}
std::int32_t rd_i32(const std::uint8_t* p) noexcept {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(p[0]) |
                                     (static_cast<std::uint32_t>(p[1]) << 8) |
                                     (static_cast<std::uint32_t>(p[2]) << 16) |
                                     (static_cast<std::uint32_t>(p[3]) << 24));
}
std::uint32_t rd_u32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(rd_i32(p));
}

// 掩码 → 通道宽度与移位（供 16/32bpp BITFIELDS 用）。
struct MaskInfo {
    int shift = 0;
    int bits = 0;
};

bool parse_mask(std::uint32_t mask, MaskInfo& out) noexcept {
    if (mask == 0) {
        return false;
    }
    int shift = 0;
    while ((mask & 1u) == 0u) {
        mask >>= 1;
        ++shift;
    }
    int bits = 0;
    while ((mask & 1u) != 0u) {
        mask >>= 1;
        ++bits;
    }
    if (mask != 0 || bits > 16) {
        return false;  // 掩码不连续或单通道超过 16 位：不解析
    }
    out.shift = shift;
    out.bits = bits;
    return true;
}

// 按掩码抽一个通道并归一化到 0..255（bits ≥ 8 取高字节；< 8 线性放大）。
std::uint8_t channel_of(std::uint32_t pixel, const MaskInfo& m) noexcept {
    if (m.bits == 0) {
        return 0;
    }
    const std::uint32_t v = (pixel >> m.shift) & ((1u << m.bits) - 1u);
    if (m.bits >= 8) {
        return static_cast<std::uint8_t>(v >> (m.bits - 8));
    }
    return static_cast<std::uint8_t>((v * 255u) / ((1u << m.bits) - 1u));
}

}  // namespace

Result<Bitmap> decode_bmp(const std::uint8_t* data, std::size_t size) {
    // ── 文件头（14 字节）────────────────────────────────────────────────────
    if (size < 2) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);  // 连魔数都读不全
    }
    if (data[0] != 'B' || data[1] != 'M') {
        return Unexpected<ErrorCode>(ErrorCode::Unsupported);  // 魔数不对
    }
    if (size < 14) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);  // 文件头截断
    }

    // ── 信息头（≥ 40 字节才支持；12 字节 OS/2 旧头拒绝）─────────────────────
    const std::uint8_t* info = data + 14;
    const std::uint32_t info_size = rd_u32(info);
    if (info_size < 40) {
        return Unexpected<ErrorCode>(ErrorCode::Unsupported);  // OS/2 V1/V2 头
    }
    if (size < 14 + info_size) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);  // 信息头截断
    }
    const std::uint32_t pixel_offset = rd_u32(data + 10);
    const std::int32_t width = rd_i32(info + 4);
    std::int32_t height = rd_i32(info + 8);
    const std::uint16_t bpp = rd_u16(info + 14);
    const std::uint32_t compression = rd_u32(info + 16);
    const std::uint32_t clr_used = rd_u32(info + 32);
    if (width <= 0 || height == 0) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    const bool top_down = height < 0;
    if (top_down) {
        height = -height;
    }
    if (compression != 0 && compression != 3) {
        return Unexpected<ErrorCode>(ErrorCode::Unsupported);  // RLE4/RLE8 等
    }

    // BITFIELDS：头后跟 3（V4+ 可 4）个 DWORD 掩码。BI_RGB 的高位深用默认掩码。
    std::uint32_t mask_r = 0;
    std::uint32_t mask_g = 0;
    std::uint32_t mask_b = 0;
    std::uint32_t mask_a = 0;
    std::size_t cursor = 14 + info_size;
    if (compression == 3) {
        if (cursor + 12 > size) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        mask_r = rd_u32(data + cursor);
        mask_g = rd_u32(data + cursor + 4);
        mask_b = rd_u32(data + cursor + 8);
        cursor += 12;
        if (info_size >= 52 && cursor + 4 <= size) {  // V4+ 有 alpha 掩码
            mask_a = rd_u32(data + cursor);
            cursor += 4;
        }
    } else if (bpp == 16) {
        mask_r = 0x7C00u;  // RGB555 默认
        mask_g = 0x03E0u;
        mask_b = 0x001Fu;
    } else if (bpp == 32) {
        mask_r = 0x00FF0000u;  // BGRX 默认（A 忽略）
        mask_g = 0x0000FF00u;
        mask_b = 0x000000FFu;
    }

    // ── 调色板（bpp ≤ 8）───────────────────────────────────────────────────
    std::vector<std::uint32_t> palette;  // 0xRRGGBB
    if (bpp <= 8) {
        const std::uint32_t entries = clr_used != 0 ? clr_used : (1u << bpp);
        const std::size_t palette_bytes = static_cast<std::size_t>(entries) * 4;
        if (cursor + palette_bytes > size) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        palette.reserve(entries);
        for (std::uint32_t i = 0; i < entries; ++i) {
            const std::uint8_t* e = data + cursor + static_cast<std::size_t>(i) * 4;
            // BMP 调色板是 BGRA 顺序（第 4 字节保留）。
            const std::uint32_t rgb =
                (static_cast<std::uint32_t>(e[2]) << 16) | (static_cast<std::uint32_t>(e[1]) << 8) |
                static_cast<std::uint32_t>(e[0]);
            palette.push_back(rgb);
        }
        cursor += palette_bytes;
    }

    // ── 像素数据 ───────────────────────────────────────────────────────────
    const std::uint32_t row_bytes =
        ((static_cast<std::uint32_t>(width) * bpp + 31u) / 32u) * 4u;  // 行 4 字节对齐
    const std::size_t pixel_bytes =
        static_cast<std::size_t>(row_bytes) * static_cast<std::size_t>(height);
    const std::size_t data_begin = pixel_offset != 0 ? pixel_offset : cursor;
    if (data_begin == 0 || data_begin + pixel_bytes > size) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }

    Bitmap out(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 4);
    MaskInfo mr{};
    MaskInfo mg{};
    MaskInfo mb{};
    MaskInfo ma{};
    const bool masked = (bpp == 16 || bpp == 32);
    if (masked) {
        if (!parse_mask(mask_r, mr) || !parse_mask(mask_g, mg) || !parse_mask(mask_b, mb)) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        if (mask_a != 0) {
            (void)parse_mask(mask_a, ma);
        }
    }

    for (std::int32_t row = 0; row < height; ++row) {
        // bottom-up：数据首行是图像末行；top-down 直接顺序读。
        const std::size_t src_row =
            top_down ? static_cast<std::size_t>(row)
                     : static_cast<std::size_t>(height - 1 - row);
        const std::uint8_t* line = data + data_begin + src_row * row_bytes;
        const std::uint32_t dst_y = static_cast<std::uint32_t>(row);
        for (std::int32_t col = 0; col < width; ++col) {
            std::uint8_t r = 0;
            std::uint8_t g = 0;
            std::uint8_t b = 0;
            std::uint8_t a = 255;
            if (bpp == 24) {
                const std::uint8_t* px = line + static_cast<std::size_t>(col) * 3;
                b = px[0];
                g = px[1];
                r = px[2];
            } else if (bpp == 32) {
                const std::uint32_t px = rd_u32(line + static_cast<std::size_t>(col) * 4);
                r = channel_of(px, mr);
                g = channel_of(px, mg);
                b = channel_of(px, mb);
                if (ma.bits != 0) {
                    a = channel_of(px, ma);
                }
            } else if (bpp == 16) {
                const std::uint16_t px = rd_u16(line + static_cast<std::size_t>(col) * 2);
                r = channel_of(px, mr);
                g = channel_of(px, mg);
                b = channel_of(px, mb);
            } else {  // 1/4/8 调色板
                std::uint32_t index = 0;
                if (bpp == 8) {
                    index = line[col];
                } else if (bpp == 4) {
                    const std::uint8_t byte = line[col / 2];
                    index = (col % 2 == 0) ? static_cast<std::uint32_t>(byte >> 4)
                                           : static_cast<std::uint32_t>(byte & 0x0Fu);
                } else {  // bpp == 1
                    const std::uint8_t byte = line[col / 8];
                    index = static_cast<std::uint32_t>((byte >> (7 - (col % 8))) & 1u);
                }
                if (index >= palette.size()) {
                    return Unexpected<ErrorCode>(ErrorCode::ParseError);
                }
                const std::uint32_t rgb = palette[index];
                r = static_cast<std::uint8_t>((rgb >> 16) & 0xFFu);
                g = static_cast<std::uint8_t>((rgb >> 8) & 0xFFu);
                b = static_cast<std::uint8_t>(rgb & 0xFFu);
            }
            out.set(static_cast<std::uint32_t>(col), dst_y, 0, r);
            out.set(static_cast<std::uint32_t>(col), dst_y, 1, g);
            out.set(static_cast<std::uint32_t>(col), dst_y, 2, b);
            out.set(static_cast<std::uint32_t>(col), dst_y, 3, a);
        }
    }
    return out;
}

}  // namespace termchart::image
