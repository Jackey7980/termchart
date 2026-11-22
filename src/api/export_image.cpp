// export_image.cpp —— 位图导出高层入口实现
#include "termchart/api/export_image.hpp"

#include <cstdio>
#include <span>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/util/file.hpp"

namespace termchart::api {

namespace {

// 视频容器请求了尚未实现的码流（如 AV1）→ 显式 Unsupported。与"位图为空/格式未实现"
// 区分开，避免调用方把"这个码流编不出来"误判成"你的图是空的"。
bool is_unsupported_video_codec(image::ImageFormat format, const image::EncodeOptions& opts) {
    return format == image::ImageFormat::Mp4 && !image::is_video_codec_supported(opts.video_codec);
}

}  // namespace

Status save_image(const image::Bitmap& bitmap, std::string_view path,
                  image::ImageFormat format, const image::EncodeOptions& opts) {
    if (is_unsupported_video_codec(format, opts)) return ErrorCode::Unsupported;
    const std::string bytes = image::encode(bitmap, format, opts);
    if (bytes.empty()) {
        return ErrorCode::InvalidArgument;  // 空位图 / 格式未实现
    }
    const std::string path_str(path);
    std::FILE* fp = util::fopen_utf8(path_str, "wb");
    if (fp == nullptr) {
        return ErrorCode::IoError;
    }
    const Status st = write_to(fp, bytes);
    std::fclose(fp);
    return st;
}

Status save_image(std::span<const image::Bitmap> frames, std::string_view path,
                  image::ImageFormat format, const image::EncodeOptions& opts) {
    if (is_unsupported_video_codec(format, opts)) return ErrorCode::Unsupported;
    const std::string bytes = image::encode(frames, format, opts);
    if (bytes.empty()) {
        return ErrorCode::InvalidArgument;  // 空帧序列 / 格式未实现
    }
    const std::string path_str(path);
    std::FILE* fp = util::fopen_utf8(path_str, "wb");
    if (fp == nullptr) {
        return ErrorCode::IoError;
    }
    const Status st = write_to(fp, bytes);
    std::fclose(fp);
    return st;
}

}  // namespace termchart::api
