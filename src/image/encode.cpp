// encode.cpp —— 统一位图编码分发器实现
#include "termchart/image/encode.hpp"

#include <array>

#include "termchart/image/gif_encode.hpp"
#include "termchart/image/jpeg_encode.hpp"
#include "termchart/image/mp4.hpp"
#include "termchart/image/png_encode.hpp"
#include "termchart/image/webp_encode.hpp"

namespace termchart::image {

std::string encode(const Bitmap& bitmap, ImageFormat format, const EncodeOptions& opts) {
    std::array<const Bitmap, 1> one{bitmap};
    return encode(std::span<const Bitmap>{one}, format, opts);
}

std::string encode(std::span<const Bitmap> frames, ImageFormat format, const EncodeOptions& opts) {
    if (frames.empty()) {
        return std::string{};
    }
    switch (format) {
        case ImageFormat::Png:
            return encode_png(frames[0]);
        case ImageFormat::Jpeg:
            return encode_jpeg(frames[0], opts.quality);
        case ImageFormat::Mp4:
            return encode_mp4(frames, opts);
        case ImageFormat::Gif:
            return encode_gif(frames, opts);
        case ImageFormat::Webp:
            return encode_webp(frames, opts);
            // Avif 落地后在此追加 case
    }
    return std::string{};
}

}  // namespace termchart::image
