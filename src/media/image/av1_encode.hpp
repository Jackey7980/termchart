// av1_encode.hpp —— AV1 一期最小纵切：8x8 单帧 IVF（内部接口，尚未接入公开分发）
#ifndef TERMCHART_SRC_IMAGE_AV1_ENCODE_HPP
#define TERMCHART_SRC_IMAGE_AV1_ENCODE_HPP

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "termchart/media/image/bitmap.hpp"

namespace termchart::image {

// 8x8 4:2:0 的期望解码画面；色度在本期固定为中性灰。
struct Av1MinimalYuv {
    std::array<std::uint8_t, 64> y{};
    std::array<std::uint8_t, 16> u{};
    std::array<std::uint8_t, 16> v{};
};

// 目前只接受 8x8 的 RGB/RGBA Bitmap；其它尺寸返回空串（fail-loud 由调用方处理）。
// expected 非空时写入与编码器同一条反量化 / 反变换路径算出的期望像素。
std::string encode_av1_minimal_ivf(const Bitmap& frame, Av1MinimalYuv* expected = nullptr);

// 真实图像输入的帧内编码选项。lambda_div 越大越偏画质；rdo_mode / rdo_partition
// 关掉可固定模式与划分，作为 RDO 效果的回归基线。
struct Av1FrameEncodeOptions {
    int base_q_idx = 100;
    int lambda_div = 4;
    bool rdo_mode = true;
    bool rdo_partition = true;
    // 当前公开的整帧入口默认只做预测选择：残差熵在多块纹理帧上
    // 尚未逐像素闭合，不作为一致性承诺。
    bool prediction_only = true;
};

// 解码端应得到的 YUV420 像素（已裁到源帧尺寸）。
struct Av1FrameYuv {
    std::vector<std::uint8_t> y;
    std::vector<std::uint8_t> u;
    std::vector<std::uint8_t> v;
};

// 真实 Bitmap → 4:2:0 → RDO 帧内编码 → 单帧 IVF。
// 当前按完整 64x64 超级块铺帧，宽高必须是 64 的倍数且至少一维为 64；
// 其它尺寸返回空串。expected 非空时写入编码器重建像素，供解码器逐像素对拍。
// prediction_only=true（默认）只做预测选择，已与 libdav1d 逐像素对拍闭合；
// false 开启残差写码，目前仍属实验路径。
std::string encode_av1_ivf(const Bitmap& frame,
                           const Av1FrameEncodeOptions& options = {},
                           Av1FrameYuv* expected = nullptr);

// 多帧 IVF：每帧独立帧内编码为 KEY_FRAME，第一帧写序列头、后续帧省略；
// 各帧尺寸必须一致，且满足单帧入口的 64 倍数约束。任一帧不合规返回空串。
std::string encode_av1_ivf(std::span<const Bitmap> frames,
                           const Av1FrameEncodeOptions& options = {});

}  // namespace termchart::image

#endif  // TERMCHART_SRC_IMAGE_AV1_ENCODE_HPP
