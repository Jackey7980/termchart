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

// 8 抽头 1/16 像素相位滤波器的第 phase 相（phase 为 0..15 的 1/16 样点相位）。
// small=true 取 dav1d 的 3+FILTER 窄块系数表（宽或高 <= 4 时使用）。
const std::int16_t* interp_filter_taps(InterpFilter filter, int phase,
                                       bool small = false);

// 单平面块拷贝/亚像素插值的最小基元：整数样点偏移 + 0..15 的 1/16 相位。
// 供色度使用（色度的 1/16 相位取自亮度 MV 的低 4 位，无法用 1/8 单位表达）。
// 限制：块宽 w <= 8（当前帧间流水线只产出 8x8 块）。
// small_h/small_v 对应 dav1d GET_H_FILTER/GET_V_FILTER 对 w<=4/h<=4 的选表。
void inter_predict_block_phase(const std::uint8_t* ref, int ref_stride,
                               int ref_width, int ref_height,
                               std::uint8_t* dst, int dst_stride,
                               int w, int h, int x0, int y0,
                               int phase_x, int phase_y,
                               InterpFilter filter = InterpFilter::Regular,
                               bool small_h = false, bool small_v = false);

// 单平面块拷贝/亚像素插值。mv_x/mv_y 以 1/8 平面样点为单位；
// src 坐标按帧边界钳制（等价于参考帧的 progressive 边界扩展）。
void inter_predict_block(const std::uint8_t* ref, int ref_stride,
                         int ref_width, int ref_height,
                         std::uint8_t* dst, int dst_stride,
                         int w, int h, int mv_x, int mv_y,
                         InterpFilter filter = InterpFilter::Regular);

// 中间精度版本：输出保留 4 位小数的 int16 预测（无符号值域 0..4080），
// 只在复合（AVG / 掩码混合 / inter-intra）时使用——解码器做复合混合时
// 用的正是这个中间量，先降到 8 位再平均会在 1 个码值上产生偏差。
void inter_predict_block_mid(const std::uint8_t* ref, int ref_stride,
                             int ref_width, int ref_height,
                             std::int16_t* dst, int dst_stride,
                             int w, int h, int x0, int y0,
                             int phase_x, int phase_y,
                             InterpFilter filter = InterpFilter::Regular);

// 两路中间精度预测的平均：dst = clip((a + b + 16) >> 5)。
void inter_compound_average_mid(const std::int16_t* a, const std::int16_t* b,
                                std::uint8_t* dst, int n);

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

// 8x8 全局运动 warp：matrix 是 16.16 定点矩阵，alpha/beta/gamma/delta
// 是帧头 subexp 解出的 15 位定点参数；输出与解码端逐像素一致。
void warp_affine_8x8(const std::uint8_t* ref, int ref_stride,
                     int ref_width, int ref_height,
                     std::uint8_t* dst, int dst_stride,
                     int x0, int y0, const std::int32_t matrix[6],
                     int alpha, int beta, int gamma, int delta,
                     int coord_scale = 1);

// —— 块级 warped motion（motion_mode = MM_WARP）的 PARAMETERS 推导 ——
// 码流里不为此读任何符号：矩阵由邻居 MV 最小二乘拟合得到（dav1d derive_warpmv 同构）。
// matrix 语义与 warp_affine_8x8 的入参一致；affine=false 表示拟合失败或剪切参数
// 越界，解码端会退回 IDENTITY（普通整块 MC），此时不要走 warp 滤波器。
struct WarpMotionParams {
    std::int32_t matrix[6]{0, 0, 0, 0, 0, 0};
    int alpha = 0;
    int beta = 0;
    int gamma = 0;
    int delta = 0;
    bool affine = false;
};

// 一个邻居采样点：dx4 / dy4 是沿邻居边界的 4x4 偏移（dav1d 的 off），
// bw4 / bh4 是邻居块尺寸（4x4 单位），sx / sy 选邻居所在的边与角
// （上边邻居 (1,-1)、左边邻居 (-1,1)、左上角 (-1,-1)、右上角 (1,-1)）。
// 采样点坐标 = 16*(2*dx4 + sx*bw4) - 8，纵向同理。
struct WarpSample {
    int dx4 = 0;
    int dy4 = 0;
    int bw4 = 0;
    int bh4 = 0;
    int sx = 1;
    int sy = 1;
    int mv_x = 0;
    int mv_y = 0;
};

// dav1d derive_warpmv + dav1d_find_affine_int + dav1d_get_shear_params 的等价实现：
// 采样点经 |mvd| 阈值筛掉后做最小二乘，拟合 16.16 定点 2x3 矩阵并解出 alpha/beta/gamma/delta。
WarpMotionParams derive_warp_motion(const WarpSample* samples, int np,
                                    int bw4, int bh4, int mv_x, int mv_y,
                                    int bx4, int by4);

// OBMC：把邻近块的预测以 4x4 权重窗叠加到当前块（范围 0..32）。
void inter_obmc_blend(std::uint8_t* dst, int dst_stride,
                      const std::uint8_t* neighbor, int neighbor_stride,
                      const std::uint8_t weights[4][4], int w, int h);

// OBMC 上边界软混合：h_px 是重叠区像素高（块高的一半，上限 32，只取 2/4/8/16/32），
// 只覆盖当前块前 (h_px*3)>>2 行，第 y 行邻居权重取 dav1d_obmc_masks[h_px + y]
// （0..64 级），混合式 (cur*(64-m) + neighbor*m + 32) >> 6。
void inter_obmc_blend_top(std::uint8_t* dst, int dst_stride,
                          const std::uint8_t* neighbor, int neighbor_stride,
                          int w, int h_px);

// 4:2:0 色度的两个平面一起预测。入参 mv 是亮度 MV（1/8 亮度样点）：
// 色度整数偏移 = mv >> 4，色度 1/16 相位 = mv & 15（与规范/参考解码器一致）。
void inter_predict_chroma(const std::uint8_t* u, const std::uint8_t* v,
                          int ref_stride, int ref_w, int ref_h,
                          std::uint8_t* dst_u, std::uint8_t* dst_v,
                          int dst_stride, int w, int h, int mv_x, int mv_y,
                          InterpFilter filter = InterpFilter::Regular);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_IMAGE_CODEC_AV1_PREDICT_INTER_PREDICT_HPP_
