// headers.hpp —— AV1 最小 sequence_header / frame_header（8-bit 4:2:0, still picture）
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_HEADERS_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_HEADERS_HPP

#include <string>

namespace termchart::image::codec::av1 {

struct SequenceHeaderConfig {
    int width = 8;
    int height = 8;
    // false 保持 reduced_still_picture_header 的极简帧内序列头；true 写出
    // 可承载 INTER_FRAME 的完整序列头（无 timing/order_hint，screen tools 自适应）。
    bool inter_frames = false;
    // enable_masked_compound：楔形/距离加权复合必须打开；关闭时复合块恒为 AVG。
    bool masked_compound = false;
    // enable_interintra_compound：单参考帧间块可与帧内预测混合。
    bool interintra = false;
    // enable_order_hint / jnt_comp：距离加权复合（DISTWTD）需要两者同时打开。
    bool order_hint = false;
    bool jnt_comp = false;
    int order_hint_bits = 1;
};

struct FrameHeaderConfig {
    int base_q_idx = 100;  // >0 ⇒ 非无损，tx_mode 可由 1 bit 选 LARGEST
    int width = 8;         // 仅用于推导 tile_info 的 max_log2_cols / max_log2_rows
    int height = 8;
    int frame_type = 0;       // 0 = KEY_FRAME，1 = INTER_FRAME
    int global_mv_x = 0;      // INTER_FRAME 的全局平移 MV（1/8 像素）
    int global_mv_y = 0;
    // 参考槽 0 的全局运动类型：0 = IDENTITY（默认，块 MV 恒零），
    // 1 = TRANSLATION（global_mv_x/y 为 1/8 像素）、2 = ROTZOOM、3 = AFFINE。
    int global_mv_type = 0;
    // ROTZOOM/AFFINE 的 15 位定点参数与 1/64 像素平移。
    int gm_alpha = 0, gm_beta = 0, gm_gamma = 0, gm_delta = 0;
    int gm_trans_x = 0, gm_trans_y = 0;
    // 帧级亚像素插值滤波器：0 规律/EIGHTTAP_REGULAR、1 平滑/SMOOTH、
    // 2 锐利/MULTITAP_SHARP、3 双线性/BILINEAR（所有块共用，暂不写 switchable）。
    int interp_filter = 0;
    // 环路去块滤波：loop_filter_level[0]/[1] 取同一个亮度强度 y，
    // level_u/level_v 取 uv；sharpness 为 0..7。
    int loop_filter_level_y = 0;
    int loop_filter_level_uv = 0;
    int loop_filter_sharpness = 0;
    // using_qmatrix：打开后按 4 bit 写 qm_y / qm_u（separate_uv_delta_q=0 时
    // qm_v = qm_u）。0..14 是规范量化矩阵预设，15 表示常权重 32（不使用矩阵）。
    bool using_qmatrix = false;
    int qm_y = 15;
    int qm_u = 15;
    // switchable_comp_refs：1 时每个帧间块都要读 is_compound 位（单参考块写 0），
    // 双参考块才可能进入 AVG / 楔形掩码复合。
    bool compound_refs = false;
    // is_motion_mode_switchable：1 时每个有重叠邻居的帧间块都要读 motion_mode
    // 语法。warp_motion 位在 error_resilient 下不写，于是解码端只读一个单符号
    // obmc CDF：1 ⇒ MM_OBMC（用邻居 MV 的预测沿边界软混合）。
    bool switchable_motion_mode = false;
    // 与序列头 order_hint 联动；非 0 时写 frame_offset 与 ref_order_hint。
    bool order_hint = false;
    int order_hint_bits = 1;
    int frame_offset = 0;
    // 逐块 warp 需要 error_resilient=0 + warp_motion=1。
    bool error_resilient = true;
    bool warp_motion = false;
    int primary_ref_frame = 0;
};

// 返回 OBU payload（未加 OBU 头）；reduced_still_picture_header=1 分支。
std::string write_sequence_header(const SequenceHeaderConfig& cfg);
std::string write_frame_header(const FrameHeaderConfig& cfg);

// 阶段五：非 reduced_still_picture_header 的序列头与 INTER_FRAME 帧头。
std::string write_sequence_header_inter(const SequenceHeaderConfig& cfg);
std::string write_frame_header_inter(const FrameHeaderConfig& cfg);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_HEADERS_HPP
