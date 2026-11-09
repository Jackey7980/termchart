// cu_coding.hpp —— 8x8 帧内 CU 的完整 CABAC bin 序列
//
// 组合顺序依据 FFmpeg libavcodec/hevc/hevcdec.c：
//   hls_coding_unit -> intra_prediction_unit -> hls_transform_tree -> hls_transform_unit
// 固定配置：I slice / 4:2:0 / pcm_enabled_flag=0 / PART_2Nx2N /
// max_transform_hierarchy_depth_intra>=1（8x8 CU 拆成 4 个 4x4 TU）。
//
// bin 序列：CU 头 -> transform_tree 根(cbf_cb, cbf_cr)
//           -> 4 个 z 序 4x4 TU 的 cbf_luma 与亮度 residual_coding
//           -> 第 4 个 TU 之后追加 U/V 两个 4x4 色度 residual_coding。
// 色度残差位置来自 FFmpeg hevcdec.c:1484-1506：MaxTb=4x4 时亮度 4x4 TU
// 的 blk_idx==3 分支在 xBase/yBase 处做色度预测与残差；色度用 DCT-II、
// cbf_cb/cr 在根节点读，语法 c_idx>0 的上下文偏移见 residual_coding.hpp。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_CU_CODING_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_CU_CODING_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cabac_syntax.hpp"
#include "residual_coding.hpp"  // ScanIdx

namespace termchart::image::codec::hevc {

struct CuIntra8x8 {
    bool part_2nx2n = true;  // 只支持 true；false（PART_NxN）需 4 套 PU 语法，未实现
    bool prev_intra_luma_pred_flag = true;
    int mpm_idx = 0;                   // prev=1 时有效，0..2
    int rem_intra_luma_pred_mode = 0;  // prev=0 时有效，0..31
    int intra_chroma_pred_mode = 4;    // 0..4
    int cbf_cb = 0;
    int cbf_cr = 0;
    // 4 个 z 序 4x4 亮度量化 level（含符号，按 scan_idx 对应的扫描序）；
    // nullptr 视为全零。扫描方式由本 CU 的亮度模式决定，必须传给残差语法层。
    ScanIdx scan_idx = kScanDiag;
    const std::int16_t* luma4x4[4] = {nullptr, nullptr, nullptr, nullptr};
    // U/V 各一个 4x4 色度 TU（8x8 亮度 CU → 4x4 色度）；nullptr/全零时 cbf 应为 0。
    // DM 模式下色度预测模式等于亮度模式，故系数扫描与亮度共用同一套表。
    const std::int16_t* chroma4x4[2] = {nullptr, nullptr};
};

// 返回空序列表示输入超出当前支持范围（残差编码失败）。
std::vector<Bin> encode_cu_intra_8x8(const CuIntra8x8& cu);

struct CuIntra8x8Decoded {
    bool part_2nx2n = true;
    bool prev_intra_luma_pred_flag = true;
    int mpm_idx = 0;
    int rem_intra_luma_pred_mode = 0;
    int intra_chroma_pred_mode = 4;
    int cbf_cb = 0;
    int cbf_cr = 0;
    int cbf_luma[4] = {0, 0, 0, 0};
    std::int16_t luma[4][16] = {};
    std::int16_t chroma[2][16] = {};
};

// shape 提供不由 bin 决定的 SPS 开关。
struct CuIntra8x8Shape {
    // MaxTb=4x4（本 SPS 配置）时解码端不读 split_transform_flag，
    // 该字段仅保留历史语义，解码不再消费它。
    bool emit_split_transform_flag = true;
    bool emit_chroma_cbf = true;
    ScanIdx scan_idx = kScanDiag;  // 不由 bin 决定，由 CU 的亮度模式推导
};

bool decode_cu_intra_8x8(const Bin* bins, std::size_t count, const CuIntra8x8Shape& shape,
                         CuIntra8x8Decoded* out, std::size_t* consumed);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_CU_CODING_HPP
