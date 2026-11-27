// loop_restoration.hpp —— AV1 Loop Restoration：Wiener 与 Sgrproj 8-bit 定点核。
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_POSTPROC_LOOP_RESTORATION_HPP_
#define TERMCHART_SRC_IMAGE_CODEC_AV1_POSTPROC_LOOP_RESTORATION_HPP_

#include <cstdint>

namespace termchart::image::codec::av1 {

struct Av1WienerParams {
    std::int8_t h[3]{};  // 左/中/右对称三次抽头
    std::int8_t v[3]{};
};

// 7 抽头水平 + 5 抽头竖直 Wiener；边界按钳制扩展，输出 clip 到 8-bit。
void av1_lr_wiener_unit(std::uint8_t* dst, int dst_stride,
                        const std::uint8_t* src, int src_stride,
                        int w, int h, const Av1WienerParams& params);

struct Av1SgrParams {
    int s0 = 0, s1 = 0;  // 3x3 / 5x5 的平滑度（规范 sgr_params 表）
    int w0 = 0, w1 = 0;  // 混合权重（w1 隐含 w0+w1+w2=128）
};

// 引导滤波：3x3 与 5x5 方框均值/方差求 a/b，再按权重混合。
void av1_lr_sgrproj_unit(std::uint8_t* dst, int dst_stride,
                         const std::uint8_t* src, int src_stride,
                         int w, int h, const Av1SgrParams& params);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_POSTPROC_LOOP_RESTORATION_HPP_
