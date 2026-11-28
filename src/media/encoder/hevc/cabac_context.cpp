// cabac_context.cpp —— HEVC CABAC 上下文表与状态转移（A3-1 从 hevc_encode.cpp 迁入）
#include "cabac_context.hpp"

namespace termchart::image::codec::hevc {
namespace {

// rangeTabLps：64 状态 × 4 个 range 区间（HEVC 9-bit range 模型，range∈[256,510]）。
const std::uint8_t kLpsTable[64][4] = {
    {128,176,208,240}, {128,167,197,227}, {128,158,187,216}, {123,150,178,205},
    {116,142,169,195}, {111,135,160,185}, {105,128,152,175}, {100,122,144,166},
    { 95,116,137,158}, { 90,110,130,150}, { 85,104,123,142}, { 81, 99,117,135},
    { 77, 94,111,128}, { 73, 89,105,122}, { 69, 85,100,116}, { 66, 80, 95,110},
    { 62, 76, 90,104}, { 59, 72, 86, 99}, { 56, 69, 81, 94}, { 53, 65, 77, 89},
    { 51, 62, 73, 85}, { 48, 59, 69, 80}, { 46, 56, 66, 76}, { 43, 53, 63, 72},
    { 41, 50, 59, 69}, { 39, 48, 56, 65}, { 37, 45, 54, 62}, { 35, 43, 51, 59},
    { 33, 41, 48, 56}, { 32, 39, 46, 53}, { 30, 37, 43, 50}, { 29, 35, 41, 48},
    { 27, 33, 39, 45}, { 26, 31, 37, 43}, { 24, 30, 35, 41}, { 23, 28, 33, 39},
    { 22, 27, 32, 37}, { 21, 26, 30, 35}, { 20, 24, 29, 33}, { 19, 23, 27, 31},
    { 18, 22, 26, 30}, { 17, 21, 25, 28}, { 16, 20, 23, 27}, { 15, 19, 22, 25},
    { 14, 18, 21, 24}, { 14, 17, 20, 23}, { 13, 16, 19, 22}, { 12, 15, 18, 21},
    { 12, 14, 17, 20}, { 11, 14, 16, 19}, { 11, 13, 15, 18}, { 10, 12, 15, 17},
    { 10, 12, 14, 16}, {  9, 11, 13, 15}, {  9, 11, 12, 14}, {  8, 10, 12, 14},
    {  8,  9, 11, 13}, {  7,  9, 11, 12}, {  7,  9, 10, 12}, {  7,  8, 10, 11},
    {  6,  8,  9, 11}, {  6,  7,  9, 10}, {  6,  7,  8,  9}, {  2,  2,  2,  2}
};

// transIdxMps / transIdxLps（状态转移）。
const std::uint8_t kNextMPS[64] = {
    1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
    17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
    33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
    49,50,51,52,53,54,55,56,57,58,59,60,61,62,62,63
};
const std::uint8_t kNextLPS[64] = {
    0,0,1,2,2,4,4,5,6,7,8,9,9,11,11,12,
    13,13,15,15,16,16,18,18,19,19,21,21,22,22,23,24,
    24,25,26,26,27,27,28,29,29,30,30,30,31,32,32,33,
    33,33,34,34,35,35,35,36,36,36,37,37,37,38,38,63
};

}  // namespace

// FFmpeg (ff_h264_mlps_state+128)[s] 的等价实现，s 为 state=(pStateIdx<<1)|valMPS。
std::uint8_t next_state(int s, bool lps) {
    const int p = (s >> 1) & 63, v = s & 1;
    if (!lps) return static_cast<std::uint8_t>((kNextMPS[p] << 1) | v);
    return static_cast<std::uint8_t>((kNextLPS[p] << 1) | (p == 0 ? 1 - v : v));
}

// FFmpeg ff_h264_lps_range[2*(range&0xC0)+s] == rangeTabLps[pStateIdx][(range>>6)&3]。
int range_lps(int s, int range) {
    return kLpsTable[(s >> 1) & 63][(range >> 6) & 3];
}

// FFmpeg ff_h264_norm_shift[x]：x∈[0,511]，把 range 左移回 [256,510]。
int norm_shift(int x) {
    if (x <= 0) return 9;
    int bits = 0;
    for (int t = x; t; t >>= 1) ++bits;
    const int s = 9 - bits;
    return s > 0 ? s : 0;
}

void CabacContext::init(int slice_qp) {
    for (auto& c : ctx) c.state = 0;   // pStateIdx=0, valMPS=0
    auto init_one = [&](int idx, int init_value) {
        // HEVC 9.3.1.1 / FFmpeg cabac_init_state()（libavcodec/hevc/cabac.c）：
        //   m = (initValue >> 4) * 5 - 45,  n = (initValue & 15) * 8 - 16
        //   pre = 2 * (((m * Clip3(0,51,SliceQpY)) >> 4) + n) - 127
        //   pre ^= pre >> 31;  if (pre > 124) pre = 124 + (pre & 1)
        // 结果直接就是 CABAC 状态字节 2*pStateIdx + valMPS。
        // 注意【2 *】因子：早期从截断片段抄成 X - 127，会让全部上下文初始状态错位。
        const int m = (init_value >> 4) * 5 - 45;
        const int n = ((init_value & 15) << 3) - 16;
        const int qp = slice_qp < 0 ? 0 : (slice_qp > 51 ? 51 : slice_qp);
        int pre = 2 * (((m * qp) >> 4) + n) - 127;   // 算术右移，与 FFmpeg 一致
        pre ^= pre >> 31;
        if (pre > 124) pre = 124 + (pre & 1);
        ctx[idx].state = static_cast<std::uint8_t>(pre);
    };
    // init_type = 2 - slice_type = 0（I 片）对应的 init_values：
    //   SPLIT_CODING_UNIT_FLAG_OFFSET=2 → ctxInc 0/1/2 取 139/141/157
    //   PART_MODE_OFFSET=13          → 首 bin 取 184
    // 本地 ctx[] 编号只需与 code_cu 中算出的 ctxInc 一致，不必等于 FFmpeg 的绝对偏移。
    init_one(0, 139);   // split_cu_flag, ctxInc=0
    init_one(1, 141);   // split_cu_flag, ctxInc=1
    init_one(2, 157);   // split_cu_flag, ctxInc=2
    init_one(13, 184);  // part_mode 首 bin（bin "1" → PART_2Nx2N）
}

namespace {

// FFmpeg libavcodec/hevc/cabac.c 的 init_values[0]（I slice，init_type=0）。
// 按 CABAC_ELEMS 中 NUM_BINS>0 的元素顺序展开；CNU（未用上下文）写 154。
// 共 179 项，与 kCtx* 绝对偏移一一对应。P/B 的 init_values[1..2] 在阶段二补。
constexpr int kInitValueCount = 179;
constexpr std::uint8_t kInitValues[kInitValueCount] = {
    153, 200,                                            // sao_merge_flag, sao_type_idx
    139, 141, 157,                                      // split_coding_unit_flag
    154,                                                // cu_transquant_bypass_flag
    154, 154, 154,                                      // skip_flag (CNU)
    154, 154, 154,                                      // cu_qp_delta
    154,                                                // pred_mode_flag (CNU)
    184, 154, 154, 154,                                 // part_mode
    184,                                                // prev_intra_luma_pred_flag
    63, 139,                                            // intra_chroma_pred_mode
    154, 154,                                           // merge_flag, merge_idx (CNU)
    154, 154, 154, 154, 154,                            // inter_pred_idc (CNU)
    154, 154, 154, 154,                                 // ref_idx_l0/l1 (CNU)
    154, 154, 154, 154,                                 // abs_mvd_greater0/1 (CNU)
    154, 154,                                           // mvp_lx_flag, no_residual_data_flag
    153, 138, 138,                                      // split_transform_flag
    111, 141,                                           // cbf_luma
    94, 138, 182, 154, 154,                             // cbf_cb/cr
    139, 139, 139, 139, 139, 139,                       // transform_skip / rdpcm flags
    110, 110, 124, 125, 140, 153, 125, 127, 140,        // last_sig_coeff_x_prefix
    109, 111, 143, 127, 111, 79, 108, 123, 63,
    110, 110, 124, 125, 140, 153, 125, 127, 140,        // last_sig_coeff_y_prefix
    109, 111, 143, 127, 111, 79, 108, 123, 63,
    91, 171, 134, 141,                                  // significant_coeff_group_flag
    111, 111, 125, 110, 110, 94, 124, 108, 124, 107,    // significant_coeff_flag (44)
    125, 141, 179, 153, 125, 107, 125, 141, 179, 153,
    125, 107, 125, 141, 179, 153, 125, 140, 139, 182,
    182, 152, 136, 152, 136, 153, 136, 139, 111, 136,
    139, 111, 141, 111,
    140, 92, 137, 138, 140, 152, 138, 139, 153, 74,     // coeff_abs_level_greater1 (24)
    149, 92, 139, 107, 122, 152, 140, 179, 166, 182,
    140, 227, 122, 197,
    138, 153, 136, 167, 152, 152,                       // coeff_abs_level_greater2 (6)
    154, 154, 154, 154, 154, 154, 154, 154,             // log2_res_scale_abs
    154, 154,                                           // res_scale_sign_flag
    154, 154,                                           // cu_chroma_qp_offset_*
};

// FFmpeg libavcodec/hevc/cabac.c 的 init_values[1]（P slice，init_type=1）。
// 分组顺序与 init_values[0] 完全一致，逐项对应 CABAC_ELEMS 的累加偏移。
constexpr std::uint8_t kInitValuesP[kInitValueCount] = {
    153, 185,                                            // sao_merge_flag, sao_type_idx
    107, 139, 126,                                      // split_coding_unit_flag
    154,                                                // cu_transquant_bypass_flag
    197, 185, 201,                                      // skip_flag
    154, 154, 154,                                      // cu_qp_delta
    149,                                                // pred_mode_flag
    154, 139, 154, 154,                                 // part_mode
    154,                                                // prev_intra_luma_pred_flag
    152, 139,                                           // intra_chroma_pred_mode
    110,                                                // merge_flag
    122,                                                // merge_idx
    95, 79, 63, 31, 31,                                 // inter_pred_idc
    153, 153, 153, 153,                                 // ref_idx_l0/l1
    140, 198, 140, 198,                                 // abs_mvd_greater0/1
    168, 79,                                            // mvp_lx_flag, no_residual_data_flag
    124, 138, 94,                                       // split_transform_flag
    153, 111,                                           // cbf_luma
    149, 107, 167, 154, 154,                            // cbf_cb/cr
    139, 139, 139, 139, 139, 139,                       // transform_skip / rdpcm flags
    125, 110, 94, 110, 95, 79, 125, 111, 110,           // last_sig_coeff_x_prefix
    78, 110, 111, 111, 95, 94, 108, 123, 108,
    125, 110, 94, 110, 95, 79, 125, 111, 110,           // last_sig_coeff_y_prefix
    78, 110, 111, 111, 95, 94, 108, 123, 108,
    121, 140, 61, 154,                                  // significant_coeff_group_flag
    155, 154, 139, 153, 139, 123, 123, 63, 153, 166,    // significant_coeff_flag (44)
    183, 140, 136, 153, 154, 166, 183, 140, 136, 153,
    154, 166, 183, 140, 136, 153, 154, 170, 153, 123,
    123, 107, 121, 107, 121, 167, 151, 183, 140, 151,
    183, 140, 140, 140,
    154, 196, 196, 167, 154, 152, 167, 182, 182, 134,   // coeff_abs_level_greater1 (24)
    149, 136, 153, 121, 136, 137, 169, 194, 166, 167,
    154, 167, 137, 182,
    107, 167, 91, 122, 107, 167,                        // coeff_abs_level_greater2 (6)
    154, 154, 154, 154, 154, 154, 154, 154,             // log2_res_scale_abs
    154, 154,                                           // res_scale_sign_flag
    154, 154,                                           // cu_chroma_qp_offset_*
};

// FFmpeg libavcodec/hevc/cabac.c 的 init_values[2]（B slice，init_type=2）。
// 分组顺序与 init_values[0]/[1] 完全一致，逐项对应 CABAC_ELEMS 的累加偏移。
// 抄录来源：build/cabac_81.c 第 255-331 行（逐行核对，共 179 项）。
constexpr std::uint8_t kInitValuesB[kInitValueCount] = {
    153, 160,                                            // sao_merge_flag, sao_type_idx
    107, 139, 126,                                      // split_coding_unit_flag
    154,                                                // cu_transquant_bypass_flag
    197, 185, 201,                                      // skip_flag
    154, 154, 154,                                      // cu_qp_delta
    134,                                                // pred_mode_flag
    154, 139, 154, 154,                                 // part_mode
    183,                                                // prev_intra_luma_pred_flag
    152, 139,                                           // intra_chroma_pred_mode
    154,                                                // merge_flag
    137,                                                // merge_idx
    95, 79, 63, 31, 31,                                 // inter_pred_idc
    153, 153, 153, 153,                                 // ref_idx_l0/l1
    169, 198, 169, 198,                                 // abs_mvd_greater0/1
    168, 79,                                            // mvp_lx_flag, no_residual_data_flag
    224, 167, 122,                                      // split_transform_flag
    153, 111,                                           // cbf_luma
    149, 92, 167, 154, 154,                            // cbf_cb/cr
    139, 139, 139, 139, 139, 139,                       // transform_skip / rdpcm flags
    125, 110, 124, 110, 95, 94, 125, 111, 111,          // last_sig_coeff_x_prefix
    79, 125, 126, 111, 111, 79, 108, 123, 93,
    125, 110, 124, 110, 95, 94, 125, 111, 111,          // last_sig_coeff_y_prefix
    79, 125, 126, 111, 111, 79, 108, 123, 93,
    121, 140, 61, 154,                                  // significant_coeff_group_flag
    170, 154, 139, 153, 139, 123, 123, 63, 124, 166,    // significant_coeff_flag (44)
    183, 140, 136, 153, 154, 166, 183, 140, 136, 153,
    154, 166, 183, 140, 136, 153, 154, 170, 153, 138,
    138, 122, 121, 122, 121, 167, 151, 183, 140, 151,
    183, 140, 140, 140,
    154, 196, 167, 167, 154, 152, 167, 182, 182, 134,   // coeff_abs_level_greater1 (24)
    149, 136, 153, 121, 136, 122, 169, 208, 166, 167,
    154, 152, 167, 182,
    107, 167, 91, 107, 107, 167,                        // coeff_abs_level_greater2 (6)
    154, 154, 154, 154, 154, 154, 154, 154,             // log2_res_scale_abs
    154, 154,                                           // res_scale_sign_flag
    154, 154,                                           // cu_chroma_qp_offset_*
};

}  // namespace

void CabacContext::init_hevc(int slice_qp, int init_type) {
    const int qp = slice_qp < 0 ? 0 : (slice_qp > 51 ? 51 : slice_qp);
    const std::uint8_t* values = (init_type == kInitTypeB)
                                     ? kInitValuesB
                                     : ((init_type == kInitTypeP) ? kInitValuesP : kInitValues);
    for (int i = 0; i < kContextCount; ++i) ctx[i].state = 0;
    // 与 FFmpeg cabac_init_state() 逐字一致。
    for (int i = 0; i < kInitValueCount; ++i) {
        const int v = values[i];
        const int m = (v >> 4) * 5 - 45;
        const int n = ((v & 15) << 3) - 16;
        int pre = 2 * (((m * qp) >> 4) + n) - 127;
        pre ^= pre >> 31;
        if (pre > 124) pre = 124 + (pre & 1);
        ctx[i].state = static_cast<std::uint8_t>(pre);
    }
}

}  // namespace termchart::image::codec::hevc
