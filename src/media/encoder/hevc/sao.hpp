// sao.hpp —— HEVC 样本自适应偏移（7.3.9.2 sao() + 8.7.3 滤波过程）
//
// 对齐 FFmpeg hevcdec.c:hls_sao_param / cabac.c:sao_*_decode /
// libavcodec/h26x/h2656_sao_template.c（BO/EO 逐式同构）。
//
// 覆盖范围（与本编码器当前码流形态一致）：
//   * 4:2:0 8bit、单 slice、CTB=64、无 tile/WPP
//   * sao_type_idx ∈ {0=NOT_APPLIED, 1=BAND, 2=EDGE}
//   * PPS range extension 的 log2_sao_offset_scale_luma/chroma 恒 0
//     （本编码器不写 pps_range_extension 的 SAO 缩放，offset 按原值使用）
//
// 语法要点（7.3.9.2，逐条与 hevcdec.c:1247-1292 对齐）：
//   if (slice_sao_luma || slice_sao_chroma) {
//       if (rx > 0) sao_merge_left_flag        (ctx 0)
//       if (ry > 0 && !merge_left) sao_merge_up_flag  (ctx 0)
//   }
//   for c_idx in 0..2:
//       if (!slice_sao_flag[c_idx]) { type=NOT_APPLIED; continue; }
//       c_idx==2 ? type_idx[2]=type_idx[1] : sao_type_idx_luma/chroma (ctx 1 + bypass)
//       if type==NOT_APPLIED continue
//       4 × sao_offset_abs（bypass 截断一元）
//       if type==BAND: 4 × sao_offset_sign（bypass，仅 abs!=0）；sao_band_position（5 bypass）
//       else if c_idx != 2: sao_eo_class（2 bypass）
//   merge=1 时后续语法【全部不写】，整 CTB 继承邻居参数（SET_SAO 语义）。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_SAO_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_SAO_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cabac_syntax.hpp"

namespace termchart::image::codec::hevc {

enum : int {
    kSaoNotApplied = 0,
    kSaoBand = 1,
    kSaoEdge = 2,
};

inline constexpr int kCtxSaoMergeFlag = 0;  // FFmpeg CABAC_SAO_MERGE_FLAG_OFFSET
inline constexpr int kCtxSaoTypeIdx = 1;   // FFmpeg CABAC_SAO_TYPE_IDX_OFFSET

// 单个 CTB、单个分量的 SAO 参数（7.3.9.2 的语法元素 + 8.7.3 的推导值）。
struct SaoCtbParams {
    int type_idx[3] = {0, 0, 0};
    int offset_abs[3][4] = {};
    int offset_sign[3][4] = {};
    int band_position[3] = {0, 0, 0};
    int eo_class[3] = {0, 0, 0};
    // 推导值：offset_val[c][0] 恒 0；c_idx==2 时 band/eo 继承 c_idx==1。
    int offset_val[3][5] = {};

    bool is_active(int c) const { return type_idx[c] != kSaoNotApplied; }
};

// 8.7.3.1 的推导（等价于 hevcdec.c:1295-1305）：由 abs/sign/type 得到带符号的
// offset_val，并按 log2_sao_offset_scale 放大（本编码器恒 0，参数留作扩展）。
void sao_derive_offsets(SaoCtbParams& p, int log2_scale_luma = 0,
                        int log2_scale_chroma = 0);

// 把 sao() 语法写成 bin 序列。merge 时只写 merge flag（后续语法不出现）。
// has_left/has_up 对应 rx>0 / ry>0；slice_luma/slice_chroma 对应 slice header 开关。
void put_sao_params(std::vector<Bin>& out, const SaoCtbParams& p,
                    bool slice_luma, bool slice_chroma, bool has_left,
                    bool has_up, bool merge_left, bool merge_up);

// 对偶回读：按 hevcdec.c:hls_sao_param 的顺序读回并重建参数（含 merge 继承）。
// left/up 指向已解出的邻居参数（has_left/has_up 为 false 时可为 nullptr）。
bool get_sao_params(BinCursor& c, bool slice_luma, bool slice_chroma,
                    bool has_left, bool has_up, const SaoCtbParams* left,
                    const SaoCtbParams* up, SaoCtbParams* out,
                    bool* merge_left = nullptr, bool* merge_up = nullptr);

// ── 像素滤波（8.7.3.3 band offset / 8.7.3.4 edge offset）────────────────
// src 必须是 SAO 之前的重建（即 DBF 输出），dst 是 SAO 输出；两者不得部分重叠。
// src/dst 同用画面绝对坐标；dst_x0/dst_y0 是 dst 坐标原点偏移（默认 0 = 绝对写回，
// 传 CTB 左上角即写入一块「局部」缓冲，编码端 RD 试算 SSE 时用它避免整帧拷贝）。
// 边界规则：EO 需要两个邻居，任一在画面外则**不修改**该样本（HEVC 8.7.3.4；
// 画面边界样本的可用邻居不足时原样输出）。
void sao_filter_ctb(const SaoCtbParams& p, int c_idx, int x0, int y0, int ctb_w,
                    int ctb_h, const std::uint8_t* src, int src_stride,
                    std::uint8_t* dst, int dst_stride, int pic_w, int pic_h,
                    int dst_x0 = 0, int dst_y0 = 0);

// ── 编码端：整帧参数决策 ─────────────────────────────────────────────────
// 三个平面都是 8bit；chroma 尺寸由 width/height 折半（4:2:0）。
struct SaoFrameInput {
    int width = 0;   // 亮度宽度（64 对齐）
    int height = 0;  // 亮度高度（64 对齐）
    const std::uint8_t* src[3] = {nullptr, nullptr, nullptr};
    const std::uint8_t* rec[3] = {nullptr, nullptr, nullptr};  // DBF 输出
    int stride[3] = {0, 0, 0};
};

// 整帧 SAO 参数表：逐 CTB 一个条目（光栅序），merge_left/up 决定码流语法。
struct SaoTable {
    bool slice_luma = false;
    bool slice_chroma = false;
    int ctb_w = 0;
    int ctb_h = 0;
    std::vector<SaoCtbParams> params;
    std::vector<std::uint8_t> merge_left;
    std::vector<std::uint8_t> merge_up;
    const SaoCtbParams* at(int ctb_x, int ctb_y) const {
        if (ctb_x < 0 || ctb_y < 0 || ctb_x >= ctb_w || ctb_y >= ctb_h) return nullptr;
        return &params[static_cast<std::size_t>(ctb_y) * static_cast<std::size_t>(ctb_w) +
                       static_cast<std::size_t>(ctb_x)];
    }
};

// 逐 CTB 做 RD 决策（BO 全 32 个 band 起点、EO 4 个方向各一候选，与「不启用」和
// 「merge 邻居」比较，代价 = SSE + λ·bins）。lambda_luma/chroma 由调用方按 QP 给。
void compute_sao_table(const SaoFrameInput& in, double lambda_luma,
                       double lambda_chroma, SaoTable* table);

// 按参数表把 rec 逐 CTB 滤波到 dst（dst 尺寸/stride 与 rec 一致，可同缓冲原地：
// 因为逐 CTB 读的是独立 src 缓冲，调用方须保证 src != dst）。
void apply_sao_table(const SaoFrameInput& in, const SaoTable& table,
                     std::uint8_t* const dst[3], const int dst_stride[3]);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_SAO_HPP
