// decoder.cpp —— 图像格式嗅探与解码分发
#include "termchart/image/decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "termchart/image/bmp.hpp"
#include "termchart/image/png.hpp"

#ifdef TERMCHART_HAVE_STB_IMAGE
// stb_image 是单头库：只在一个 TU 里定义 STB_IMAGE_IMPLEMENTATION 才生成实现。
// 本文件即该唯一点（同时也是 decode_stb 的所在），受 TERMCHART_HAVE_STB_IMAGE 守卫。
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

namespace termchart::image {
namespace {

bool matches(const std::uint8_t* data, std::size_t size, const char* magic,
             std::size_t length) noexcept {
    return size >= length && std::memcmp(data, magic, length) == 0;
}

#ifdef TERMCHART_HAVE_STB_IMAGE
// 走 vendored stb_image 解码任意受支持格式为 RGBA。GIF 只取首帧（静态图库语义）。
Result<Bitmap> decode_stb(const std::uint8_t* data, std::size_t size) {
    int w = 0, h = 0, ch = 0;
    stbi_uc* px = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &ch, 4);
    if (px == nullptr || w <= 0 || h <= 0) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    Bitmap out(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 4);
    const std::size_t bytes =
        static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u;
    if (bytes > 0) {
        std::memcpy(out.data(), px, bytes);  // stb 4 通道即 RGBA，与 Bitmap 布局一致
    }
    stbi_image_free(px);
    return out;
}
#endif

}  // namespace

ImageFormat sniff_image_format(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size < 4) {
        return ImageFormat::Unknown;
    }
    if (matches(data, size, "\x89PNG\r\n\x1a\n", 8)) {
        return ImageFormat::Png;
    }
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return ImageFormat::Jpeg;
    }
    if (matches(data, size, "GIF8", 4)) {
        return ImageFormat::Gif;
    }
    if (matches(data, size, "BM", 2)) {
        return ImageFormat::Bmp;
    }
    if (matches(data, size, "RIFF", 4) && size >= 12 &&
        std::memcmp(data + 8, "WEBP", 4) == 0) {
        return ImageFormat::WebP;
    }
    return ImageFormat::Unknown;
}

const char* image_format_name(ImageFormat format) noexcept {
    switch (format) {
        case ImageFormat::Unknown:
            return "unknown";
        case ImageFormat::Png:
            return "png";
        case ImageFormat::Jpeg:
            return "jpeg";
        case ImageFormat::Gif:
            return "gif";
        case ImageFormat::Bmp:
            return "bmp";
        case ImageFormat::WebP:
            return "webp";
    }
    return "unknown";
}

Result<Bitmap> decode_image(const std::uint8_t* data, std::size_t size) {
    switch (sniff_image_format(data, size)) {
        case ImageFormat::Png:
            return decode_png(data, size);
        case ImageFormat::Bmp:
            return decode_bmp(data, size);
#ifdef TERMCHART_HAVE_STB_IMAGE
        case ImageFormat::Jpeg:
        case ImageFormat::Gif:
        case ImageFormat::WebP:
            return decode_stb(data, size);
#endif
        case ImageFormat::Unknown:
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        default:
            // 识别得出格式但没有解码器：如实说 Unsupported，不做"猜着解"。
            return Unexpected<ErrorCode>(ErrorCode::Unsupported);
    }
}

}  // namespace termchart::image
