// encode.cpp —— 统一位图编码分发器实现
#include "termchart/image/encode.hpp"

#include "termchart/image/jpeg_encode.hpp"
#include "termchart/image/png_encode.hpp"

namespace termchart::image {

std::string encode(const Bitmap& bitmap, ImageFormat format, const EncodeOptions& opts) {
    switch (format) {
        case ImageFormat::Png:
            return encode_png(bitmap);
        case ImageFormat::Jpeg:
            return encode_jpeg(bitmap, opts.quality);
            // Webp / Gif / Mp4 / Avif 落地后在此追加 case
    }
    return std::string{};
}

}  // namespace termchart::image
