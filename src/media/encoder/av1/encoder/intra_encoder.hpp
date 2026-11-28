// intra_encoder.hpp —— 真实图像输入的帧内编码主循环（8-bit 4:2:0）。
//
// 与探针的区别：这里吃的是真实像素，走真实的「预测 → 残差 → 变换 → 量化 →
// 反量化 → 反变换」闭环，RDO 的 R 用区间编码器实测、D 用重建像素实测 SSD。
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_ENCODER_INTRA_ENCODER_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_ENCODER_INTRA_ENCODER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "termchart/media/image/bitmap.hpp"

namespace termchart::image::codec::av1 {

// 8-bit 4:2:0 平面帧。
struct PlaneFrame420 {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> y;
    std::vector<std::uint8_t> u;
    std::vector<std::uint8_t> v;

    int chroma_width() const noexcept { return (width + 1) >> 1; }
    int chroma_height() const noexcept { return (height + 1) >> 1; }
};

struct IntraEncodeOptions {
    int base_q_idx = 100;
    // lambda = dq_ac^2 / lambda_div；越小越偏画质。
    int lambda_div = 4;
    bool rdo_mode = true;       // 关掉则固定 SMOOTH_PRED（仅用于回归基线）
    bool rdo_partition = true;  // 关掉则 16x16 单元一律 SPLIT 到 4 个 8x8
    // 仅做预测选择、不写残差：当前残差熵在多块纹理帧上尚未逐像素闭合，
    // 该模式保证码流可被 libdav1d 逐像素一致重建。
    bool prediction_only = false;
};

struct IntraEncodeResult {
    std::string tile_data;  // 未包 OBU 的 tile 载荷
    PlaneFrame420 recon;    // 与解码端推得的重建像素（用于逐像素对拍）
};

// RGBA / RGB → BT.601 全范围 4:2:0。
PlaneFrame420 bitmap_to_yuv420(const Bitmap& bitmap);

// 真实编码：超块遍历 + 16x16 NONE/SPLIT 决策 + 8x8 模式/量化 RDO。
IntraEncodeResult encode_intra_frame(const PlaneFrame420& src,
                                     const IntraEncodeOptions& options);

}  // namespace termchart::image::codec::av1

#endif
