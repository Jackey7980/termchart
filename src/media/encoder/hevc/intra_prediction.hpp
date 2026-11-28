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
// 色度预测复用同一套参考样本构造/35 模式逻辑：x0,y0,pic_w,pic_h 仍传【亮度】
// 坐标与尺寸（FFmpeg ff_hevc_set_neighbour_available 对色度也传 xBase/yBase 与
// 亮度 CTB 尺度），POS 则按 >>1 采样独立的色度重建平面；色度 c_idx>0 不做
// DC 首行/首列边界滤波（pred_template.c 条件为 c_idx==0 && nTbS<32）。
struct ChromaPredPlane {
    const std::uint8_t* rec = nullptr;  // nullptr ⇒ 走亮度路径
    int stride = 0;
};

// rec 为帧内预测用的【滤波前重建】平面，stride 为行距（字节）。
// 末尾 chroma 默认为空 ⇒ 亮度路径行为逐位不变。
void predict_intra_luma_4x4(int mode, const std::uint8_t* rec, int stride,
                            int x0, int y0,
                            int pic_w, int pic_h, std::uint8_t* dst,
                            ChromaPredPlane chroma = {});

// 与尺寸无关的入口：log2_size 2..5 ⇒ 块边长 4/8/16/32。参考样本构造、可用性、
// 1-2-1 平滑（仅亮度 size>=8）、DC/角度首行首列后处理（size<32）均按块尺寸切换。
// 4x4 与 predict_intra_luma_4x4 逐像素一致（后者内部已委托同一实现）。
void predict_intra_luma(int mode, const std::uint8_t* rec, int stride,
                        int x0, int y0, int log2_size,
                        int pic_w, int pic_h, std::uint8_t* dst,
                        ChromaPredPlane chroma = {});

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

// 该模式要写多少 bin 的模式语法（与 cabac_syntax.cpp 逐条同构）：
//   prev_intra_luma_pred_flag 1 个 ctx bin；
//   prev=1 → put_mpm_idx：idx 0 写 1 bin，idx 1/2 写 2 bin；
//   prev=0 → put_rem_intra_luma_pred_mode：5 个 bypass bin。
// 与模式无关的常量语法（part_mode / intra_chroma_pred_mode / split_transform）
// 不计入，因为它们在候选之间是同一个常数，不参与比较。
int mode_signal_bins(int target_mode, const MpmCandidates& cands);

// 约束式 RDO 的相对失真余量（默认 0.05，可由 TERMCHART_HEVC_RDO_SLACK 覆盖）。
// 模式决策与 CBF 决策共用同一个值，避免两处口径漂移。
double rdo_distortion_slack();

// 逐 CU 的亮度帧内模式决策：对 35 个模式各模拟一遍
// 「预测 → 变换量化 → 逆量化重建」，按重建块对源块的 SSD 选最小。
//
// tu_log2 必须是【真实使用的 TU 尺寸】：2 表示 8x8 CU 分裂成 4 个 4x4 TU，
// 3/4/5 表示单个 8/16/32 TU。模拟必须与真实编码同尺寸、同 TU 次序，否则
// 大 TU 下会用 4x4 的失真模型挑模式（历史缺陷）。只影响模式选择，不产生新
// 语法，任意返回值都能被 FFmpeg 正确解码。
//
// rd_lambda > 0 时切到率失真口径 J = SSD + λ·(模式语法 bin + 残差 bin)，
// bin 数全部由 encode_residual 实算；rd_lambda == 0 是默认路径，代价退化为
// 纯 SSD（逐位不变，码流与既有基线字节级一致）。mpm_cands 为 nullptr 时只计
// 残差 bin：CU 分裂评估层没有 tab_ipm，而模式语法 bin 在分裂/停止两路都存在，
// 真正区分两路的是残差 bin，故这是合理口径而非缺陷。
// 要求 (x0,y0) 按 CU 对齐、整块在图像内；越界返回 INTRA_DC(1)。
int choose_intra_luma_mode(const std::uint8_t* src, int src_stride,
                           const std::uint8_t* rec, int rec_stride,
                           int x0, int y0, int pic_w, int pic_h, int qp, int tu_log2,
                           double rd_lambda = 0.0,
                           const MpmCandidates* mpm_cands = nullptr);

// 旧的 4x4 TU 档接口，等价于 tu_log2=2。
int choose_intra_luma_mode_8x8(const std::uint8_t* src, int src_stride,
                               const std::uint8_t* rec, int rec_stride,
                               int x0, int y0, int pic_w, int pic_h, int qp);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_INTRA_PREDICTION_HPP
