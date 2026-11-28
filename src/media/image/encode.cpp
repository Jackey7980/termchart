// encode.cpp —— 统一位图编码分发器实现
//
// 按 ImageFormat 分流到 media 门面的注册名；实际编码与容器封装由 media 层
// 的注册表路由，底层 encode_* 保持原样，字节输出不变。
#include "termchart/media/image/encode.hpp"

#include <array>
#include <string_view>

#include "termchart/media/media.hpp"

namespace termchart::image {
namespace {

constexpr std::string_view format_name(ImageFormat format) noexcept {
    switch (format) {
        case ImageFormat::Png:  return "png";
        case ImageFormat::Jpeg: return "jpeg";
        case ImageFormat::Mp4:  return "mp4";
        case ImageFormat::Gif:  return "gif";
        case ImageFormat::Webp: return "webp";
        case ImageFormat::Av1:  return "av1";
    }
    return {};
}

}  // namespace

std::string encode(const Bitmap& bitmap, ImageFormat format, const EncodeOptions& opts) {
    std::array<const Bitmap, 1> one{bitmap};
    return encode(std::span<const Bitmap>{one}, format, opts);
}

std::string encode(std::span<const Bitmap> frames, ImageFormat format, const EncodeOptions& opts) {
    if (frames.empty()) {
        return std::string{};
    }
    return media::encode(frames, format_name(format), opts);
}

bool is_video_codec_supported(VideoCodec codec) noexcept {
    switch (codec) {
        case VideoCodec::H264:
            return true;
        case VideoCodec::H265:
            return true;
        case VideoCodec::AV1:
            return true;  // 帧内路径已落地（IVF 容器；prediction_only 档）
    }
    return false;
}

}  // namespace termchart::image
