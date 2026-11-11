// cabac_syntax.hpp —— HEVC CABAC 语法元素的上下文映射与 bin 序列
//
// 头文件只暴露接口、POD 与常量；ctxInc 推导与 bin 登记规则在 .cpp。
// 两套写入路径并存：
//   · write_*(CabacEncoder&) —— I_PCM 档的区间反解引擎（保持字节级不变）；
//   · put_*/get_* —— 真压缩档的「语法元素 -> Bin 序列」，由 cabac_arith 落 bits。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_SYNTAX_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_SYNTAX_HPP

#include <cstddef>
#include <vector>

namespace termchart::image::codec::hevc {

class CabacEncoder;

// ctx >= 0：上下文自适应 bin（ctx 为 FFmpeg CABAC_ELEMS 绝对偏移）；
// ctx == kBypassCtx：等概率 bypass bin。供 cabac_syntax 与 residual_coding 共用。
struct Bin {
    int ctx;
    int val;
};
inline constexpr int kBypassCtx = -1;

// 只读游标，用于按 FFmpeg *_decode 语义独立回读 bin 序列并校验 ctx。
class BinCursor {
public:
    BinCursor(const Bin* bins, std::size_t count) : bins_(bins), count_(count) {}
    bool decision(int expect_ctx, int& val);
    bool bypass(int& val);
    std::size_t consumed() const { return pos_; }
    // 供子语法（如 residual_coding）从当前位置继续读。
    const Bin* current() const { return bins_ + pos_; }
    std::size_t remaining() const { return count_ - pos_; }
    void advance(std::size_t n) { pos_ += n; }

private:
    const Bin* bins_;
    std::size_t count_;
    std::size_t pos_ = 0;
};

// ── I_PCM 档（区间反解引擎）──────────────────────────────────────────────
// PART_MODE 首 bin 的上下文（FFmpeg PART_MODE_OFFSET）。
inline constexpr int kPartModeCtx = 13;

// split_cu_flag 的 ctxInc（HEVC 9.3.4.2.2）：均匀四分树全拆到 8x8 叶时，
// 「左侧/上方有已编码 CU」等价于绝对坐标 > 0，故 ctxInc = (x0>0)+(y0>0)。
int split_cu_flag_ctx(int x0, int y0);

void write_split_cu_flag(CabacEncoder& enc, int x0, int y0, int bin);
void write_part_mode_2nx2n(CabacEncoder& enc);
void write_pcm_flag(CabacEncoder& enc, int bit);
void write_end_of_slice_segment_flag(CabacEncoder& enc, int bit);

// ── 真压缩档：单个 CU 语法元素的 bin 化 / 对偶回读 ──────────────────────
// 每个 put_* 只负责一个语法元素，CU 内的组合顺序由上层按 hls_coding_unit 组织。
void put_split_cu_flag(std::vector<Bin>& out, int ctx_inc, int value);
void put_part_mode_intra(std::vector<Bin>& out, bool part_2nx2n);
void put_prev_intra_luma_pred_flag(std::vector<Bin>& out, int value);
void put_mpm_idx(std::vector<Bin>& out, int idx);                 // 0..2
void put_rem_intra_luma_pred_mode(std::vector<Bin>& out, int mode);  // 0..31
void put_intra_chroma_pred_mode(std::vector<Bin>& out, int mode);     // 0..4
void put_cbf_luma(std::vector<Bin>& out, int trafo_depth, int value);
void put_cbf_cb_cr(std::vector<Bin>& out, int c_idx, int trafo_depth, int value);
void put_split_transform_flag(std::vector<Bin>& out, int log2_trafo_size, int value);

// transform_skip_flag（ffmpeg cabac.c:850-853）：ctx = 47 + !!c_idx。
// 只在 pps->transform_skip_enabled_flag 且 log2_trafo_size <=
// log2_max_transform_skip_block_size 时读写，位置在残差语法最前面
// （last_significant_coeff_x/y_prefix 之前，cabac.c:1138-1140）。
void put_transform_skip_flag(std::vector<Bin>& out, int c_idx, int value);
bool get_transform_skip_flag(BinCursor& c, int c_idx, int& value);

bool get_split_cu_flag(BinCursor& c, int ctx_inc, int& value);
bool get_part_mode_intra(BinCursor& c, bool& part_2nx2n);
bool get_prev_intra_luma_pred_flag(BinCursor& c, int& value);
bool get_mpm_idx(BinCursor& c, int& idx);
bool get_rem_intra_luma_pred_mode(BinCursor& c, int& mode);
bool get_intra_chroma_pred_mode(BinCursor& c, int& mode);
bool get_cbf_luma(BinCursor& c, int trafo_depth, int& value);
bool get_cbf_cb_cr(BinCursor& c, int c_idx, int trafo_depth, int& value);
bool get_split_transform_flag(BinCursor& c, int log2_trafo_size, int& value);

// ── CU 内元素的组合顺序（按 FFmpeg libavcodec/hevc/hevcdec.c 逐行核对）──
// I slice / 4:2:0 / pcm_enabled_flag=0 / transquant_bypass=0 / cu_qp_delta=0 /
// PART_2Nx2N（side=1）下的 coding_unit() 到 hls_transform_tree() 根节点：
//   part_mode(仅 log2_cb_size==MinCbLog2SizeY)
//   prev_intra_luma_pred_flag
//     ==1 -> mpm_idx(bypass 0..2)；==0 -> rem_intra_luma_pred_mode(5 bypass)
//   intra_chroma_pred_mode(4:2:0 只读 1 个)
//   split_transform_flag(仅当 SPS/PPS 条件允许时)
//   cbf_cb, cbf_cr(trafo_depth==0 必读), cbf_luma(intra 必读)
// 之后由上层按 split_transform_flag 追加 4 个子树或 4x4 残差 bin 序列。
struct CuIntraBins {
    bool emit_part_mode = true;
    bool part_2nx2n = true;
    bool prev_intra_luma_pred_flag = true;
    int mpm_idx = 0;                  // prev=1 时有效，0..2
    int rem_intra_luma_pred_mode = 0;  // prev=0 时有效，0..31
    int intra_chroma_pred_mode = 4;   // 0..4
    bool emit_split_transform_flag = false;
    int log2_trafo_size = 3;
    int trafo_depth = 0;
    bool split_transform_flag = false;
    bool emit_chroma_cbf = true;      // trafo_depth==0 恒为 true
    int cbf_cb = 0, cbf_cr = 0, cbf_luma = 1;
};

std::vector<Bin> encode_cu_intra_bins(const CuIntraBins& p);
// shape 提供不由 bin 决定的开关（emit_*、log2_trafo_size、trafo_depth）。
bool decode_cu_intra_bins(BinCursor& c, const CuIntraBins& shape, CuIntraBins* out);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_SYNTAX_HPP
