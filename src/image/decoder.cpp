// decoder.cpp —— 图像格式嗅探与解码分发
#include "termchart/image/decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "termchart/image/png.hpp"

namespace termchart::image {
namespace {

bool matches(const std::uint8_t* data, std::size_t size, const char* magic,
             std::size_t length) noexcept {
    return size >= length && std::memcmp(data, magic, length) == 0;
}

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
        case ImageFormat::Unknown:
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        default:
            // 识别得出格式但没有解码器：如实说 Unsupported，不做"猜着解"。
            return Unexpected<ErrorCode>(ErrorCode::Unsupported);
    }
}

}  // namespace termchart::image
