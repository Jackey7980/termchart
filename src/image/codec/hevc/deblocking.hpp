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
// 已知近似（**不进码流**，只影响编码端重建帧 `rec`）：只滤亮度、bS 恒取 2、
// 8 样本段共用单个 tc（未做 FFmpeg 的 tc[0]/tc[1] 双段），也没有色度通路。因此
// `rec` 与 ffmpeg 解码输出存在 maxdiff≈4 的差异（实测 QP26/渐变图，128x64）。
// 要逐像素一致必须传 CU/TU 边界图、按 8.7.2.4 逐边求 bS 并补色度滤波，
// 属阶段三（P3-1）的正式工作。当前正确性门禁走 DBF 关闭路径：
// 编码设 TERMCHART_HEVC_NO_DBLOCK=1、解码加 `-skip_loop_filter all`，Y 平面 maxdiff=0。
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

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_DEBLOCKING_HPP
