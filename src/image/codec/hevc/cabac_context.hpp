// cabac_context.hpp —— HEVC CABAC 上下文模型接口
//
// 头文件只暴露 POD 与纯函数；Table 9-37 / 9-38 / 9-39 的具体数值留在 .cpp，
// 不向语法层泄漏。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_CONTEXT_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_CONTEXT_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// state = (pStateIdx << 1) | valMPS（HEVC 9.3.4.2 约定）。
struct CtxModel {
    std::uint8_t state = 0;
};

// HEVC_CONTEXTS：FFmpeg CABAC_ELEMS 的非零 NUM_BINS 之和为 179，留 4 个余量。
// 旧代码只分配 154 个，阶段一 B3 的 significant_coeff_flag / coeff_abs_level_*
// 偏移会越界，必须扩到这里。
inline constexpr int kContextCount = 183;

// FFmpeg CABAC_ELEMS 的绝对上下文偏移（I slice，init_type=0）。
inline constexpr int kCtxSplitCuFlag = 2;
inline constexpr int kCtxPartMode = 13;
inline constexpr int kCtxPrevIntraLumaPred = 17;
inline constexpr int kCtxIntraChromaPred = 18;
inline constexpr int kCtxSplitTransform = 37;
inline constexpr int kCtxCbfLuma = 40;
inline constexpr int kCtxCbfCbCr = 42;
// TRANSFORM_SKIP_FLAG_OFFSET：由 CABAC_ELEMS 表累加自洽推出
//   37(split_tf,3) + 40(cbf_luma,2) + 42(cbf_cb/cr,5) = 47(transform_skip,2)
//   + 49(rdpcm,2) + 51(rdpcm_dir,2) = 53(last_sig_x,18) ✓
// ffmpeg cabac.c:852 用 TRANSFORM_SKIP_FLAG_OFFSET + !!c_idx（亮度 47 / 色度 48）。
inline constexpr int kCtxTransformSkip = 47;
inline constexpr int kCtxLastSigCoeffX = 53;
inline constexpr int kCtxLastSigCoeffY = 71;
inline constexpr int kCtxSigCoeffGroup = 89;
inline constexpr int kCtxSigCoeffFlag = 93;
inline constexpr int kCtxCoeffAbsGreater1 = 137;
inline constexpr int kCtxCoeffAbsGreater2 = 161;

// P 片用到的帧间上下文（偏移与 ffmpeg CABAC_ELEMS 累加一致）。
// 注意 abs_mvd_greater0/1 的取值方式照抄 ffmpeg n8.1.2 的 hls_mvd_coding()：
//   greater0 两个分量共用 31，greater1 两个分量共用 34（不是 31..34 逐分量）。
// 本项目的外部解码真值就是同版本的 ffmpeg，两者必须用同一套索引才能解对。
inline constexpr int kCtxSkipFlag = 6;
inline constexpr int kCtxPredMode = 12;
inline constexpr int kCtxRefIdxL0 = 15;
inline constexpr int kCtxRefIdxL1 = 16;
inline constexpr int kCtxMergeFlag = 20;
inline constexpr int kCtxMergeIdx = 21;
inline constexpr int kCtxMvdGreater0 = 31;
inline constexpr int kCtxMvdGreater1 = 34;
inline constexpr int kCtxMvpLxFlag = 35;
inline constexpr int kCtxNoResidualData = 36;

// B 片 inter_pred_idc（5 个上下文，ffmpeg INTER_PRED_IDC_OFFSET=22）：
//   nPbW+nPbH==12 → 单 bin @ 22+4（1=BI, 0=L0）
//   否则 bin0 @ 22+ct_depth（1=BI）；0 时 bin1 @ 22+4（1=L1, 0=L0）
inline constexpr int kCtxInterPredIdc = 22;

// init_type = 2 - slice_type：I=0 / P=1 / B=2（ffmpeg cabac_init_state 的取表方式）。
inline constexpr int kInitTypeI = 0;
inline constexpr int kInitTypeP = 1;
inline constexpr int kInitTypeB = 2;

struct CabacContext {
    CtxModel ctx[kContextCount];

    // I_PCM 档沿用旧的自定义 ctx 编号（0/1/2=split，13=part_mode），
    // 与 cabac_engine 的区间反解配套；行为不能变，否则 G0 基线会破。
    void init(int slice_qp);
    // 阶段一 B3 用的标准绝对偏移初始化（FFmpeg cabac_init_state）。
    void init_hevc(int slice_qp, int init_type = 0);

    std::uint8_t state(int i) const { return ctx[i].state; }
    void set_state(int i, std::uint8_t s) { ctx[i].state = s; }
};

int range_lps(int state, int range);
std::uint8_t next_state(int state, bool lps);
int norm_shift(int x);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_CONTEXT_HPP
