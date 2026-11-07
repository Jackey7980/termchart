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

}  // namespace termchart::image::codec::hevc
