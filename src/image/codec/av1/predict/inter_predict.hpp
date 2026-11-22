// AV1 帧间预测的像素域原语（8-bit 4:2:0）。
// 只负责“给定参考帧与 MV → 预测块”，不读码流、不做参考帧管理。
#ifndef TERMCHART_IMAGE_CODEC_AV1_PREDICT_INTER_PREDICT_HPP_
#define TERMCHART_IMAGE_CODEC_AV1_PREDICT_INTER_PREDICT_HPP_

#include <array>
#include <cstdint>

namespace termchart::image::codec::av1 {

enum class InterpFilter {
    Regular = 0,
    Smooth = 1,
    Sharp = 2,
    Bilinear = 3,
};

// 8 抽头 1/16 像素相位滤波器的第 frac 相（frac 为 MV 的 1/8 像素低位乘 2）。
const std::int16_t* interp_filter_taps(InterpFilter filter, int frac);

// 单平面块拷贝/亚像素插值。mv_x/mv_y 以 1/8 像素为单位；src 坐标按帧边界钳制。
void inter_predict_block(const std::uint8_t* ref, int ref_stride,
                         int ref_width, int ref_height,
                         std::uint8_t* dst, int dst_stride,
                         int w, int h, int mv_x, int mv_y,
                         InterpFilter filter = InterpFilter::Regular);

// 复合预测：两路预测等权平均（Round2Signed(...,1)）与距离加权。
void inter_compound_average(const std::uint8_t* a, const std::uint8_t* b,
                            std::uint8_t* dst, int n);
void inter_compound_distance(const std::uint8_t* a, const std::uint8_t* b,
                             int weight, std::uint8_t* dst, int n);

// 仿射/旋转缩放 warp：matrix 为 AV1 的 16.16 定点 3x3 矩阵前两行
// {m0,m1,m2,m3,m4,m5}，源坐标 = (m2-65536)*x + m3*y + m0 等，双线性取样。
void warp_affine_predict(const std::uint8_t* ref, int ref_stride,
                         int ref_width, int ref_height,
                         std::uint8_t* dst, int dst_stride,
                         int x0, int y0, int w, int h,
                         const std::int32_t matrix[6]);

// OBMC：把邻近块的预测以 4x4 权重窗叠加到当前块（范围 0..32）。
void inter_obmc_blend(std::uint8_t* dst, int dst_stride,
                      const std::uint8_t* neighbor, int neighbor_stride,
                      const std::uint8_t weights[4][4], int w, int h);

// 4:2:0 色度的两个平面一起预测（MV 右移 1，尺寸减半）。
void inter_predict_chroma(const std::uint8_t* u, const std::uint8_t* v,
                          int ref_stride, int ref_w, int ref_h,
                          std::uint8_t* dst_u, std::uint8_t* dst_v,
                          int dst_stride, int w, int h, int mv_x, int mv_y,
                          InterpFilter filter = InterpFilter::Bilinear);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_IMAGE_CODEC_AV1_PREDICT_INTER_PREDICT_HPP_
