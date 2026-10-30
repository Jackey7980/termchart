// export_image.cpp —— 位图导出高层入口实现
#include "termchart/api/export_image.hpp"

#include <cstdio>
#include <span>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/image/encode.hpp"

namespace termchart::api {

Status save_image(const image::Bitmap& bitmap, std::string_view path,
                  image::ImageFormat format, const image::EncodeOptions& opts) {
    const std::string bytes = image::encode(bitmap, format, opts);
    if (bytes.empty()) {
        return ErrorCode::InvalidArgument;  // 空位图 / 格式未实现
    }
    const std::string path_str(path);
    std::FILE* fp = std::fopen(path_str.c_str(), "wb");
    if (fp == nullptr) {
        return ErrorCode::IoError;
    }
    const Status st = write_to(fp, bytes);
    std::fclose(fp);
    return st;
}

Status save_image(std::span<const image::Bitmap> frames, std::string_view path,
                  image::ImageFormat format, const image::EncodeOptions& opts) {
    const std::string bytes = image::encode(frames, format, opts);
    if (bytes.empty()) {
        return ErrorCode::InvalidArgument;  // 空帧序列 / 格式未实现
    }
    const std::string path_str(path);
    std::FILE* fp = std::fopen(path_str.c_str(), "wb");
    if (fp == nullptr) {
        return ErrorCode::IoError;
    }
    const Status st = write_to(fp, bytes);
    std::fclose(fp);
    return st;
}

}  // namespace termchart::api
