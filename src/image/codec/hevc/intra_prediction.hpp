// intra_prediction.hpp —— HEVC 4x4 帧内预测（Planar / DC / 33 角度）+ 模式决策
//
// 两套入口分工：
//   predict_intra            —— 不依赖重建平面的 Planar/DC 参考实现（单测与平面/DC
//                               语义对拍用），top/left 显式传入。
//   predict_intra_luma_4x4   —— 编码器生产路径，覆盖 0..34 全部 35 个模式，从
//                               重建平面按 TU 原点取参考样本。
// 参考样本：top[0..n-1] 为上方列 0..n-1，top[n] 为右上；left[0..n-1] 为左侧
// 行 0..n-1，left[n] 为左下。has_top / has_left 指示参考侧是否可用（边界外推）。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_INTRA_PREDICTION_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_INTRA_PREDICTION_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// 独立的 Planar/DC 参考实现：mode 0 = Planar、1 = DC；mode>=2 按 DC 处理。
// 它不是编码器生产路径 —— 角度模式一律走 predict_intra_luma_4x4。
// dst 为 n*n 行优先预测块。
void predict_intra(int mode, int n, const std::uint8_t* top,
                   const std::uint8_t* left, bool has_top, bool has_left,
                   std::uint8_t* dst);

// 4x4 亮度帧内预测（HEVC 8.4.4.2；对齐 FFmpeg hevc/pred_template.c 的
// intra_pred(size=2) 路径）。HEVC 的帧内预测块尺寸等于 TU 尺寸（本编码器 8x8 CU
// 分 4 个 4x4 TU），故虽然 8x8 CU 是单 PU、单一亮度模式，预测仍逐 4x4 TU 进行。
// 每个 TU 用 PU 模式、以其自身 (x0,y0) 定位参考样本；邻居可用性也按 TU 原点求值
// （FFmpeg hls_transform_unit → ff_hevc_set_neighbour_available(lc, x0, y0,
// trafo_size, trafo_size)），同 CU 内先解出的 TU 已写回 rec，是合法参考源。
// rec 为帧内预测用的【滤波前重建】平面，stride 为行距（字节）。
void predict_intra_luma_4x4(int mode, const std::uint8_t* rec, int stride,
                            int x0, int y0,
                            int pic_w, int pic_h, std::uint8_t* dst);

// HEVC 8.4.4.2.3 / FFmpeg hevcdec.c:luma_intra_pred_mode 的 MPM 候选表推导。
// cand_left / cand_up 为左、上 PU 的帧内模式；不可用时由调用方传 INTRA_DC(1)。
struct MpmCandidates {
    int mode[3] = {0, 1, 26};
};

MpmCandidates derive_mpm_candidates(int cand_left, int cand_up);

// 给定欲使用的模式，求能对偶 FFmpeg 解码端的语法信号。
// FFmpeg 解码（prev=1）用未排序候选表取 cand[mpm_idx]；（prev=0）先把候选表
// 升序排序，再 mode = rem; for (i=0..2) if (mode >= sorted[i]) mode++。
// 这里按逆序对排序表求逆（rem = target; for i=2..0: if rem > sorted[i] --rem）。
struct IntraModeSignal {
    bool prev_intra_luma_pred_flag = true;
    int mpm_idx = 0;
    int rem_intra_luma_pred_mode = 0;
};

IntraModeSignal signal_intra_mode(int target_mode, const MpmCandidates& cands);

// 逐 8x8 CU 的亮度帧内模式决策（P0-4）：对 35 个模式各模拟一遍
// 「预测 → 变换量化 → 逆量化重建」，按重建块对源块的 SSD 选最小。
// 模拟使用与真实编码相同的 4 个 4x4 TU 次序，且逐 TU 写回重建，故后续 TU
// 的参考样本演化与最终编码完全一致；只影响模式选择，不产生任何新语法，
// 任意返回值都能被 FFmpeg 正确解码。暂不含 λ·bits（阶段三 RDO 再补）。
// 要求 (x0,y0) 8 对齐、x0+8<=pic_w、y0+8<=pic_h；越界返回 INTRA_DC(1)。
int choose_intra_luma_mode_8x8(const std::uint8_t* src, int src_stride,
                               const std::uint8_t* rec, int rec_stride,
                               int x0, int y0, int pic_w, int pic_h, int qp);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_INTRA_PREDICTION_HPP
