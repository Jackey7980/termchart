// mp4.hpp —— MP4（ISO-BMFF）视频封装封装入口
//
// 零依赖：自己写 box 组装，不调用 libavformat/ffmpeg。把一串视频帧（Bitmap）按
// EncodeOptions::video_codec 选内部码流编码器，再封装成可被标准播放器/ffprobe 读取的
// MP4。video_codec=H264 / H265 走自写编码器；AV1 尚未实现——此时**显式返回空串**
// （绝不静默降级成 H.264），可用 is_video_codec_supported() 预检。
#ifndef TERMCHART_IMAGE_MP4_HPP
#define TERMCHART_IMAGE_MP4_HPP

#include <span>
#include <string>

#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"

namespace termchart::image {

// 把 frames 编码并封装为 MP4 字节流。返回空串表示不支持/失败（如 video_codec 未实现）。
TERMCHART_API std::string encode_mp4(std::span<const Bitmap> frames, const EncodeOptions& opts = {});

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_MP4_HPP
