// media.hpp —— 媒体层门面：按注册名把裸帧编码为完整文件字节流。
//
// 底层 encode_* 保持原样，字节基线不变；本层只做注册表驱动的格式路由与容器
// 能力查询。C ABI 与按字符串选格式的调用方都走这里，不再手写 if-else 分流。
// 内置编码器由 register_all_builtin_codecs() 显式登记，不依赖静态初始化，
// 因此静态链接不会被裁掉。
#ifndef TERMCHART_SRC_MEDIA_MEDIA_HPP
#define TERMCHART_SRC_MEDIA_MEDIA_HPP

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/encode.hpp"

#include "termchart/media/video/muxer/i_video_muxer.hpp"

namespace termchart::media {

// 登记全部内置编码器与容器封装器。重复调用无副作用，线程安全。
void register_all_builtin_codecs();

// 按注册名编码。支持 png / jpg / jpeg / gif / webp / mp4 / av1 / ivf / avif；
// 未知名字或编码失败返回空串，调用方据此显式报错，不静默换格式。
[[nodiscard]] std::string encode(std::span<const image::Bitmap> frames,
                                 std::string_view format,
                                 const image::EncodeOptions& opts = {});

// 注册表查询：名字是否可编码、全部可编码名字。
[[nodiscard]] bool has_encoder(std::string_view format);
[[nodiscard]] std::vector<std::string> encoder_names();

// 容器元数据：该格式是单帧图还是多帧容器、默认扩展名。
[[nodiscard]] bool is_multiframe(std::string_view format);
[[nodiscard]] std::string extension_for(std::string_view format);

}  // namespace termchart::media

#endif  // TERMCHART_SRC_MEDIA_MEDIA_HPP
