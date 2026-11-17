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

    // PPS transform_skip_enabled_flag：true 时每个 cbf=1 的 TU 在残差 bin 之前
    // 多写一个 transform_skip_flag（ffmpeg cabac.c:1138-1140）。默认 false ⇒
    // bin 序列与既有码流逐字节一致。
    bool transform_skip_enabled = false;
    // 每个 TU 的 flag 值（0/1）；仅 cbf=1 时消费。色度 c_idx=1/2 → ctx 48。
    int transform_skip_luma[4] = {0, 0, 0, 0};
    int transform_skip_chroma[2] = {0, 0};

    // log2_tu_size=2 ⇒ 上面四个 4x4 TU（既有基线）；3/4/5 ⇒ 单个 8/16/32
    // 亮度 TU（MaxTb 随档位、max_transform_hierarchy_depth_intra=0，解码端
    // 不读分裂标志）。luma_large 为行主序 (1<<log2)^2 个量化 level；
    // log2>=4 时色度也是大 TU，(1<<(log2-1))^2，见 chroma_large。
    int log2_tu_size = 2;
    const std::int16_t* luma_large = nullptr;
    const std::int16_t* chroma_large[2] = {nullptr, nullptr};
    // CU 边长 > MinCb(8) 时解码端推断 PART_2Nx2N，不读 part_mode。
    bool emit_part_mode = true;
    // 自动档（MaxTb=32、max_transform_hierarchy_depth_intra=1）：每个 CU 的
    // transform_tree 根节点都要显式写 split_transform_flag。固定档下 MaxTb 就等于
    // TU 尺寸（<=3 时为 4），根节点隐式分裂/不分裂，不写这一 bin。
    // log2_root 是根节点的 log2_trafo_size（= CU 的 log2），决定 ctx=37+5-log2。
    bool emit_split_transform_flag = false;
    bool split_transform_flag = false;
    int log2_root = 3;
};

// 返回空序列表示输入超出当前支持范围（残差编码失败）。
std::vector<Bin> encode_cu_intra_8x8(const CuIntra8x8& cu);

// ── P 片帧间 CU ──────────────────────────────────────────────────────────
// bin 序列（ffmpeg hevcdec.c hls_coding_unit 2477-2600 + hls_prediction_unit）：
//   skip_flag
//   [skip=1] merge_idx  ← 到此结束（pred_mode/part_mode/merge_flag/rqt_root_cbf
//                          一个都不读）
//   [skip=0] pred_mode_flag=1(INTER) → part_mode(2Nx2N) → merge_flag
//            → merge: merge_idx
//            → AMVP : (ref_idx_l0) + mvd_coding + mvp_lx_flag
//            → rqt_root_cbf（仅 merge_flag==0 时读；2Nx2N+merge 推断为 1）
//            → transform_tree（与帧内 4x4 路径完全同构）
// 变换树差异只有两点：亮度/色度恒为 DCT-II（不是帧内的 DST-VII），扫描恒为 DIAG。
struct CuInter8x8 {
    int skip_ctx_inc = 0;   // 左/上 CU 的 skip_flag 之和，0..2（cabac.c:587-600）
    bool skip = false;      // true ⇒ P_Skip
    int max_merge_cand = 5; // slice: five_minus_max_num_merge_cand=0 ⇒ 5
    int merge_idx = 0;
    bool merge = true;      // false ⇒ AMVP
    int nb_refs_l0 = 1;     // 单参考 ⇒ 解码端不读 ref_idx_l0
    int mvd_x = 0;          // 1/4 亮度采样单位（MvdL0[0]）
    int mvd_y = 0;
    int mvp_lx_flag = 0;    // 0/1

    // 残差：skip=true 时全部忽略。
    bool emit_rqt_root_cbf = false;  // AMVP 必为 true；2Nx2N+merge 恒 false
    int rqt_root_cbf = 1;            // 0 ⇒ 无 transform_tree
    int cbf_cb = 0;
    int cbf_cr = 0;
    const std::int16_t* luma4x4[4] = {nullptr, nullptr, nullptr, nullptr};
    const std::int16_t* chroma4x4[2] = {nullptr, nullptr};
    bool transform_skip_enabled = false;
    int transform_skip_luma[4] = {0, 0, 0, 0};
    int transform_skip_chroma[2] = {0, 0};
};

// 返回空序列 = 输入非法或残差编码失败。
std::vector<Bin> encode_cu_inter_8x8(const CuInter8x8& cu);

// ── B 片帧间 CU ──────────────────────────────────────────────────────────
// prediction_unit()（HEVC 7.3.8.8）在 P 片基础上多出的部分：
//   merge_flag==1 → merge_idx（与 P 片相同）
//   merge_flag==0 → inter_pred_idc 先发，再按列表分组：
//     idc != PRED_L1 → (ref_idx_l0) + mvd_coding(L0) + mvp_l0_flag
//     idc != PRED_L0 → (ref_idx_l1) + [mvd_coding(L1) unless mvd_l1_zero_flag
//                                    && idc == PRED_BI] + mvp_l1_flag
// ctx 见 cabac_context：inter_pred_idc 22+ct_depth（小 PB 用 22+4）。
struct CuInterB8x8 {
    int skip_ctx_inc = 0;
    bool skip = false;
    int max_merge_cand = 5;
    int merge_idx = 0;
    bool merge = true;           // false ⇒ AMVP（随后发 inter_pred_idc 两组）
    int inter_pred_idc = 2;      // 0=PRED_L0 / 1=PRED_L1 / 2=PRED_BI
    int ct_depth = 2;            // 8x8 CU 在 CTB32 下的深度；决定 inter_pred_idc 的 ctx
    int nb_refs_l0 = 1;          // ==1 时解码端不读 ref_idx_l0
    int nb_refs_l1 = 1;          // ==1 时解码端不读 ref_idx_l1
    int ref_idx_l0 = 0;
    int ref_idx_l1 = 0;
    bool mvd_l1_zero_flag = false;  // 必须与 slice header 一致
    int mvd_x = 0;                  // L0 的 Mvd（1/4 亮度采样）
    int mvd_y = 0;
    int mvd_x_l1 = 0;               // L1 的 Mvd
    int mvd_y_l1 = 0;
    int mvp_lx_flag = 0;
    int mvp_lx_flag_l1 = 0;

    // 残差：与 P 片同构（skip=true 时全部忽略）。
    bool emit_rqt_root_cbf = false;
    int rqt_root_cbf = 1;
    int cbf_cb = 0;
    int cbf_cr = 0;
    const std::int16_t* luma4x4[4] = {nullptr, nullptr, nullptr, nullptr};
    const std::int16_t* chroma4x4[2] = {nullptr, nullptr};
    bool transform_skip_enabled = false;
    int transform_skip_luma[4] = {0, 0, 0, 0};
    int transform_skip_chroma[2] = {0, 0};
};

// 返回空序列 = 输入非法或残差编码失败。
std::vector<Bin> encode_cu_inter_b_8x8(const CuInterB8x8& cu);

// P 片中的帧内 CU：skip_flag=0 + pred_mode_flag=0 之后接标准帧内语法。
// 注意 P 片里 part_mode 在 MinCb 档依然要读，调用方必须让 cu.emit_part_mode=true。
std::vector<Bin> encode_cu_intra_in_p_8x8(const CuIntra8x8& cu, int skip_ctx_inc);

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
    int transform_skip_luma[4] = {0, 0, 0, 0};
    int transform_skip_chroma[2] = {0, 0};
};

// shape 提供不由 bin 决定的 SPS/PPS 开关。
struct CuIntra8x8Shape {
    // MaxTb=4x4（本 SPS 配置）时解码端不读 split_transform_flag，
    // 该字段仅保留历史语义，解码不再消费它。
    bool emit_split_transform_flag = true;
    bool emit_chroma_cbf = true;
    ScanIdx scan_idx = kScanDiag;  // 不由 bin 决定，由 CU 的亮度模式推导
    // PPS transform_skip_enabled_flag：由参数集决定，不随 bin 传输。
    bool transform_skip_enabled = false;
};

bool decode_cu_intra_8x8(const Bin* bins, std::size_t count, const CuIntra8x8Shape& shape,
                         CuIntra8x8Decoded* out, std::size_t* consumed);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_CU_CODING_HPP
