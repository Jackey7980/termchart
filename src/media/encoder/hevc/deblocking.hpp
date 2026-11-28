// deblocking.hpp —— HEVC 去块滤波（8.7.2；对齐 FFmpeg hevc/filter.c +
// h26x/h2656_deblock_template.c + hevc/dsp_template.c:hevc_loop_filter_luma）
//
// 只覆盖本编码器当前产出的码流形态：
//   * 单 slice、全帧内（PART_2Nx2N）、无帧间
//   * 8x8 CU，每个 CU 固定 4 个 4x4 TU（luma_transform_size_4x4 由 CU 尺寸推出）
//   * 全帧唯一 slice QP，无 cu_qp_delta、无 deblocking_filter_override（beta/tc
//     offset 恒 0）、无 PCM、无 transquant bypass
//
// 该形态下边界强度恒为 bS == 2（8.7.2.4：边界任一侧为帧内预测即 bS = 2），且
// 解码端只在 8 对齐的 TU 边界上施滤 —— 4x4 TU 落在 4 mod 8 位置的那条边**不进**
// bS 表（FFmpeg filter.c 的 boundary_left / boundary_upper 条件是 `!(x0 & 7)`，
// 帧内分支也不走 TU 内部 PU 边界那条路），所以整帧滤波只由固定的 beta/tc 与逐边
// 的 d 判据决定。
//
// 2026-11-13 逐像素复核（编码端开 DBF + ffmpeg 不做 -skip_loop_filter all）结论：
//   · 亮度 4x4 / 8x8 TU 档已 **maxdiff=0**（强/弱滤波 + tc 双段逐式对齐），
//     旧文档「亮度 maxdiff≈4」早已过时；
//   · 16/32 TU 档亮度差 max=2~4：固定大 TU 内部不存在 TU 边界，不该在每条
//     8 对齐边上滤波，需要按 TU 尺寸给边界周期；
//   · U/V 各档 QP26/40 差 max=4~10：缺色度通路（8.7.2.5 的 bS=2 弱滤波）。
// 本轮据此补 edge_period 与色度滤波；bS 恒 2 对全帧内码流是**正确简化**而非近似
//（8.7.2.4：边界任一侧为帧内预测即 bS=2），不需要 CU/TU 边界强度表。
// 自动档（混合 TU 尺寸）仍需逐 CU 的真实边界图，属后续工作。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_DEBLOCKING_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_DEBLOCKING_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// 8.7.2 去块滤波参数。当前编码器只会产出全 0 的 offset，字段保留是为了后续
// cu_qp_delta / deblocking_filter_override 落地时不必改接口。
struct DeblockConfig {
    int slice_qp = 26;         // SliceQpY（8 bit 时 QpBdOffsetY = 0）
    int beta_offset_div2 = 0;  // slice_beta_offset_div2
    int tc_offset_div2 = 0;    // slice_tc_offset_div2
    int chroma_qp = 26;        // qPi→qPc 之后的色度 QP（色度 tc 用）
    // 固定档的 TU 边界周期（亮度像素）：max(8, 1<<log2_tu)。HEVC 只对 8 对齐且
    // 恰为 TU/PU 边界的边滤波，16/32 TU 内部的 8 对齐边不是边界。
    int edge_period = 8;
    // 自动档（同一帧混合 TU 尺寸）用逐 8x8 网格的真实 TU 边界图代替周期：
    // 非 null 时，只有对应格置 1 的边才滤波。grid_w = ceil(width/8)。
    const std::uint8_t* vert_edge = nullptr;  // 该 8x8 格的左边界是 TU 边界
    const std::uint8_t* horz_edge = nullptr;  // 该 8x8 格的上边界是 TU 边界
    int grid_w = 0;
};

// 表 8-11 / 8-12（FFmpeg filter.c 的 betatable[52] / tctable[54]）。
// 单独暴露给单测，避免"滤出来像对但表错一格"这类回归。
int deblock_beta(int qp, int beta_offset_div2 = 0);
int deblock_tc(int qp, int boundary_strength, int tc_offset_div2 = 0);

// 对整帧亮度平面做去块滤波，原地修改。
// y 指向平面左上角，stride 为行距（字节），width / height 为当前 picture 尺寸。
// 前提：width、height 均为 8 的倍数（本编码器按 CTB 对齐，恒成立）；否则直接返回。
void deblock_frame_luma(std::uint8_t* y, int width, int height, int stride,
                        const DeblockConfig& cfg);

// 对 U/V 两个色度平面做 8.7.2.5 的 bS=2 弱滤波（只改 p0/q0 各一个样本）。
// width / height / stride 都是**色度平面**尺寸（亮度的一半）。
void deblock_frame_chroma(std::uint8_t* u, std::uint8_t* v, int width, int height,
                          int stride, const DeblockConfig& cfg);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_DEBLOCKING_HPP
