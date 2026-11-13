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

// 视频编码档位。Pcm 是一条完整独立的无损 I_PCM 路径，不参与真压缩档的分层
// 拆分（image/codec/hevc/*），始终保留为可选档与字节级黄金回归基线。
// Pcm / Intra 已落地；Inter 是阶段二占位，请求时显式返回空串，不静默降级。
enum class VideoEncodeMode {
    Pcm,    // I_PCM 无损档（默认；码流字节级稳定）
    Intra,  // 阶段一：真帧内压缩（变换/量化 + 残差 CABAC + 35 模式决策）
    Inter,  // 阶段二占位：帧间预测 + 参考帧管理尚未实现（fail-loud）
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
    VideoEncodeMode video_mode = VideoEncodeMode::Pcm;  // 默认走 I_PCM 档，保证既有码流字节级不变。
    // 真压缩档（Intra）的 slice QP，合法区间 [0,51]（越界夹到边界）。
    // 默认 26 ⇒ slice_qp_delta=0，与既有码流字节级一致；Pcm 档忽略本字段。
    int video_qp = 26;
    // 真压缩档（Intra）是否启用 transform_skip：跳过 DCT/DST，残差在像素域直接
    // 量化。默认 false ⇒ PPS transform_skip_enabled_flag=0、CU 无额外 bin，
    // 码流与既有基线逐字节一致。显式置 true 才在 PPS 打开开关，并对每个
    // cbf=1 的 TU 写 transform_skip_flag；是否真 skip 按 RD（SSD+λ·bins）
    // 逐 TU 选优，亮度与色度各自按本平面 QP 取 λ、按本块坐标算 SSD。
    // 可用尺寸由 hevc_log2_tu_size 决定：固定档声明到该档，自动档声明到 32x32；
    // 超过 4x4 时会在参数集写 RExt profile（general_profile_idc=4）+
    // sps/pps_range_extension 的 log2_max_transform_skip_block_size_minus2。
    // Pcm 档忽略本字段。
    bool hevc_transform_skip = false;
    // 真压缩档亮度 TU 边长档位（log2）：
    //   0 = 自动档（逐区域按内容在 4/8/16/32 间自适应，推荐）
    //   2 = 4x4（默认，既有字节级基线）、3 = 8x8、4 = 16x16、5 = 32x32
    // 固定档下 CU 与 TU 同尺寸（<=3 都是 8x8 CU）；自动档下 CU 树按局部复杂度
    // 停在 8/16/32，8x8 CU 再决定是否分裂成 4 个 4x4 TU。
    // SPS 的 MaxTb 与变换层次深度随档位调整；transform_skip 与任意档位都能组合
    //（4x4 沿用 PPS 默认 max=2，更大 TU 走 RExt 参数集，见上一字段）。
    // 色度 TU 恒为亮度的一半（4:2:0）。Pcm 档忽略本字段；其它值返回空串。
    int hevc_log2_tu_size = 2;
};

// 单帧编码（PNG/JPEG/静态图）。
TERMCHART_API std::string encode(const Bitmap& bitmap, ImageFormat format,
                                const EncodeOptions& opts = {});

// 多帧编码（动画 GIF / 视频 MP4 / AVIF）。单帧调用等价于把 frames 设为 1 张。
TERMCHART_API std::string encode(std::span<const Bitmap> frames, ImageFormat format,
                                const EncodeOptions& opts = {});

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_ENCODE_HPP
