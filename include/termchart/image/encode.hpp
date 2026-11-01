// encode.hpp —— 统一位图编码分发器（自包含编码器入口）
//
// 与现有 encode_png / encode_jpeg 同哲学：零外部依赖，每个编码器把 Bitmap 编码成
// 完整文件字节流（std::string）。本文件只提供一个按格式分流的薄入口，新增格式
// （WebP / GIF / MP4 / AVIF）只需：① 在 ImageFormat 追加枚举值；② 实现对应的
// encode_* 函数（返回字节流）；③ 在 encode() 里加一个 case。既有 encode_png /
// encode_jpeg 保持不变，对外契约不动。
//
// 视频/动画格式（GIF / MP4 / AVIF）走多帧重载 encode(std::span<const Bitmap>)，单帧
// 重载会退化成 1 帧调用，二者共用同一入口。
#ifndef TERMCHART_IMAGE_ENCODE_HPP
#define TERMCHART_IMAGE_ENCODE_HPP

#include <span>
#include <string>
#include <vector>

#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"

namespace termchart::image {

// 支持的位图导出格式。顺序无关；新增格式追加到末尾即可。
enum class ImageFormat {
    Png,
    Jpeg,
    Mp4,  // 视频容器 ISO-BMFF；内部码流由 EncodeOptions::video_codec 选 H264/H265/AV1
    Gif,  // 自写 GIF89a（median-cut 调色板 + LZW），支持多帧动画
    Webp, // 自写 VP8L lossless（零依赖），单帧无损；动画/有损后续增强
    // Avif 在各自编码器落地后追加
};

// 视频/动画内部码流选择（自写编码器，零依赖）。
enum class VideoCodec {
    H264,  // 自写 baseline AVC（当前已实现 I_PCM 无损档，压缩档后续叠加）
    H265,  // 自写 HEVC —— 后续阶段
    AV1,   // 自写 AV1（AVIF 容器用）—— 后续阶段
};

// 该视频码流是否已实现（encode_mp4 等能否真正产出该码流）。AV1 编码器尚未落地 → false。
// 供调用方在编码前预检：宁可显式失败，也不要"请求 AV1 却拿到 H.264 文件"这类静默降级。
[[nodiscard]] TERMCHART_API bool is_video_codec_supported(VideoCodec codec) noexcept;

// 编码选项。quality 用于有损格式；fps/loop/delay 用于动画；video_codec 用于视频容器。
struct EncodeOptions {
    int quality = 85;       // 有损格式（JPEG / WebP / MP4）质量 [1,100]；无损格式忽略。
    int fps = 25;           // 动画/视频帧率。
    int loop_count = 0;     // 0 = 无限循环（GIF）；单图忽略。
    int delay_ms = 100;     // 每帧显示时长（GIF；覆盖 fps 推算）。
    VideoCodec video_codec = VideoCodec::H264;  // MP4/AVIF 内部码流。
    int gop = 0;            // 0 = 全 I 帧（baseline intra）；>0 时启用帧间（后续阶段）。
};

// 单帧编码（PNG/JPEG/静态图）。
TERMCHART_API std::string encode(const Bitmap& bitmap, ImageFormat format,
                                const EncodeOptions& opts = {});

// 多帧编码（动画 GIF / 视频 MP4 / AVIF）。单帧调用等价于把 frames 设为 1 张。
TERMCHART_API std::string encode(std::span<const Bitmap> frames, ImageFormat format,
                                const EncodeOptions& opts = {});

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_ENCODE_HPP
