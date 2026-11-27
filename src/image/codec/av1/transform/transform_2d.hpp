// transform_2d.hpp —— AV1 全尺寸可逆变换与扫描序（8-bit 纵切）。
// 变换核按规范的正交基生成：DCT-II 用 256*cos(...)（DC 再乘 1/sqrt(2)），
// ADST 用 DST-VII 正交基，IDTX 为恒等；正/逆共用同一组基，因此浮点往返
// 误差由量化步控制，尺寸覆盖 2/4/8/16/32/64 与任意矩形组合。
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_TRANSFORM_TRANSFORM_2D_HPP_
#define TERMCHART_SRC_IMAGE_CODEC_AV1_TRANSFORM_TRANSFORM_2D_HPP_

#include <array>
#include <cstdint>

namespace termchart::image::codec::av1 {

enum class Av1Transform1d {
    Dct,
    Adst,
    Identity,
    FlipAdst,
};

// 二维反变换：coeff 为变换块存储序（行主序，w 列 h 行），输出为去量化后的
// 时域样本（含 1<<8 定标）。调用方按 `(out + 8) >> 4` 取残差。
void av1_inverse_transform_2d(const std::int16_t* coeff, std::int32_t* out,
                              int w, int h, Av1Transform1d tx,
                              Av1Transform1d ty);

// 二维正变换：residual 是时域残差，输出定标后的变换系数（供量化使用）。
void av1_forward_transform_2d(const std::int32_t* residual, std::int16_t* coeff,
                              int w, int h, Av1Transform1d tx,
                              Av1Transform1d ty);

// AV1 三种系数扫描序：0=ZIGZAG、1=HORZ、2=VERT。返回按扫描顺序排列的
// rc = row * w + col；支持正方形与 2..64 的矩形块。
void av1_scan_order(int w, int h, int scan_type, int* rc_out);

// 量化矩阵：0..14 是 15 组 8-bit 预设，15 表示不使用 qmatrix。
// 返回 64 项（行主序 8x8）权重，dav1d 的反量化式为 (dq * qm + 16) >> 5。
std::array<std::uint8_t, 64> av1_qmatrix_8x8(int level, bool chroma);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_TRANSFORM_TRANSFORM_2D_HPP_
