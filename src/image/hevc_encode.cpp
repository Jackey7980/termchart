// hevc_encode.cpp —— 自写 HEVC（H.265, Main profile）编码器（I_PCM 档）
#include "termchart/image/hevc_encode.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "codec/bitstream/bit_writer.hpp"
#include "codec/hevc/nal_unit.hpp"

namespace termchart::image {
namespace {

// ── 通用比特流基建（阶段 9 · A1）─────────────────────────────────────────
// 位写入器 / 防竞争 / Annex-B / NAL 头已下沉到 image/codec/bitstream 与
// image/codec/hevc/nal_unit。I_PCM 的编码逻辑仍自包含，只替换底层实现；
// 门禁 G0（diag_hevc.py 6/6 + 20430B/sha）保证码流字节级不变。
using codec::bitstream::BitWriter;
using codec::hevc::make_nal;

int clamp8(int v) { return codec::bitstream::clamp_u8(v); }

struct Yuv420 {
    int cw = 0, ch = 0;
    std::vector<uint8_t> y, u, v;
};

// RGB(A) → YUV420（BT.601），按 64 对齐补边（HEVC CTB = 64）。
Yuv420 to_yuv(const Bitmap& bmp) {
    const int W = static_cast<int>(bmp.width());
    const int H = static_cast<int>(bmp.height());
    const int cw = ((W + 63) / 64) * 64;
    const int ch = ((H + 63) / 64) * 64;

    const size_t cw_s  = static_cast<size_t>(cw);
    const size_t ch_s  = static_cast<size_t>(ch);
    const size_t hcw_s = cw_s / 2;            // 色度行宽（半宽）
    const size_t hch_s = ch_s / 2;            // 色度列高（半高）
    const size_t chroma_n = hcw_s * hch_s;    // U/V 平面元素数，只算一次

    Yuv420 out;
    out.cw = cw;
    out.ch = ch;
    out.y.assign(cw_s * ch_s, 0);
    out.u.assign(chroma_n, 128);
    out.v.assign(chroma_n, 128);
    for (int yy = 0; yy < H; ++yy) {
        for (int xx = 0; xx < W; ++xx) {
            const int r = bmp.at(static_cast<uint32_t>(xx), static_cast<uint32_t>(yy), 0);
            const int g = bmp.at(static_cast<uint32_t>(xx), static_cast<uint32_t>(yy), 1);
            const int bb = bmp.at(static_cast<uint32_t>(xx), static_cast<uint32_t>(yy), 2);
            const int ly = clamp8(((66 * r + 129 * g + 25 * bb + 128) >> 8) + 16);
            const size_t y_idx = static_cast<size_t>(yy) * cw_s + static_cast<size_t>(xx);
            out.y[y_idx] = static_cast<uint8_t>(ly);
            if ((xx & 1) == 0 && (yy & 1) == 0) {
                const int lu = clamp8(((-38 * r - 74 * g + 112 * bb + 128) >> 8) + 128);
                const int lv = clamp8(((112 * r - 94 * g - 18 * bb + 128) >> 8) + 128);
                const size_t idx = static_cast<size_t>(yy / 2) * hcw_s + static_cast<size_t>(xx / 2);
                out.u[idx] = static_cast<uint8_t>(lu);
                out.v[idx] = static_cast<uint8_t>(lv);
            }
        }
    }
    return out;
}

// profile_tier_level（MaxNumSubLayersMinus1 = 0，无子层循环）。
// HEVC 7.3.3：general_profile_idc==1（Main）走 else 分支 → 4 个约束标志之后是
// general_reserved_zero_44bits（44 个 0），绝不可把 Main 专属 max_*_constraint 标志写进该区。
// 注意：FFmpeg 8.1.2 对 Main 仍按 else 分支读 44 个 reserved 0，写 1 会被忽略但会污染 SPS 后续字段解析。
void profile_tier_level(BitWriter& w) {
    w.u(0, 2);   // general_profile_space
    w.u(0, 1);   // general_tier_flag
    w.u(1, 5);   // general_profile_idc = 1 (Main, 8-bit)
    w.u(2, 32);  // general_profile_compatibility_flags：bit1 置位（Main 兼容）
    w.u(1, 1);   // general_progressive_source_flag
    w.u(0, 1);   // general_interlaced_source_flag
    w.u(1, 1);   // general_non_packed_constraint_flag
    w.u(1, 1);   // general_frame_only_constraint_flag
    w.u(0, 44);  // general_reserved_zero_44bits（Main profile = else 分支，全 0）
    w.u(30, 8);  // general_level_idc = 30
}

std::string build_vps() {
    BitWriter w;
    w.u(0, 4);    // vps_video_parameter_set_id
    w.u(1, 1);    // vps_base_layer_internal_flag
    w.u(1, 1);    // vps_base_layer_available_flag
    w.u(0, 6);    // vps_max_layers_minus1
    w.u(0, 3);    // vps_max_sub_layers_minus1
    w.u(1, 1);    // vps_temporal_id_nesting_flag
    w.u(0xFFFF, 16);  // vps_reserved_0xffff_16bits
    profile_tier_level(w);
    w.u(1, 1);    // vps_sub_layer_ordering_info_present_flag
    w.ue(0);      // vps_max_dec_pic_buffering_minus1[0]
    w.ue(0);      // vps_max_num_reorder_pics[0]
    w.ue(0);      // vps_max_latency_increase_plus1[0]
    w.u(0, 6);    // vps_max_layer_id
    w.ue(0);      // vps_num_layer_sets_minus1
    w.u(0, 1);    // vps_timing_info_present_flag
    w.u(0, 1);    // vps_extension_present_flag
    return w.finish();
}

std::string build_sps(int cw, int ch) {
    BitWriter w;
    w.u(0, 4);    // sps_video_parameter_set_id
    w.u(0, 3);    // sps_max_sub_layers_minus1
    w.u(1, 1);    // sps_temporal_id_nesting_flag
    profile_tier_level(w);
    w.ue(0);      // sps_seq_parameter_set_id
    w.ue(1);      // chroma_format_idc = 1 (4:2:0)
    w.ue(static_cast<unsigned>(cw));   // pic_width_in_luma_samples
    w.ue(static_cast<unsigned>(ch));   // pic_height_in_luma_samples
    w.u(0, 1);    // conformance_window_flag = 0
    w.ue(0);      // bit_depth_luma_minus8
    w.ue(0);      // bit_depth_chroma_minus8
    w.ue(0);      // log2_max_pic_order_cnt_lsb_minus4
    w.u(0, 1);    // sps_sub_layer_ordering_info_present_flag = 0
    w.ue(0);      // sps_max_dec_pic_buffering_minus1（层 0）
    w.ue(0);      // sps_max_num_reorder_pics（层 0）
    w.ue(0);      // sps_max_latency_increase_plus1（层 0）
    // CTB = 64（MinCb=8, MaxCb=8+diff3=64）；log2_min_cb_minus3=0、diff_max_min=3。
    w.ue(0);      // log2_min_luma_coding_block_size_minus3
    w.ue(3);      // log2_diff_max_min_luma_coding_block_size
    w.ue(0);      // log2_min_luma_transform_block_size_minus2
    w.ue(3);      // log2_diff_max_min_luma_transform_block_size
    w.ue(0);      // max_transform_hierarchy_depth_inter
    w.ue(0);      // max_transform_hierarchy_depth_intra
    w.u(0, 1);    // scaling_list_enabled_flag
    w.u(0, 1);    // amp_enabled_flag
    w.u(0, 1);    // sample_adaptive_offset_enabled_flag
    w.u(1, 1);    // pcm_enabled_flag
    w.u(7, 4);    // pcm_sample_bit_depth_luma_minus1
    w.u(7, 4);    // pcm_sample_bit_depth_chroma_minus1
    w.ue(0);      // log2_min_pcm_luma_coding_block_size_minus3（PCM 块 = 8）
    w.ue(0);      // log2_diff_max_min_pcm_luma_coding_block_size
    w.u(1, 1);    // pcm_loop_filter_disabled_flag
    w.ue(0);      // num_short_term_ref_pic_sets
    w.u(0, 1);    // long_term_ref_pics_present_flag
    w.u(0, 1);    // sps_temporal_mvp_enabled_flag
    w.u(0, 1);    // strong_intra_smoothing_enabled_flag
    w.u(0, 1);    // vui_parameters_present_flag
    w.u(0, 1);    // sps_extension_present_flag
    return w.finish();
}

std::string build_pps() {
    BitWriter w;
    w.ue(0);      // pps_pic_parameter_set_id
    w.ue(0);      // pps_seq_parameter_set_id
    w.u(0, 1);    // dependent_slice_segments_enabled_flag
    w.u(0, 1);    // output_flag_present_flag
    w.u(0, 3);    // num_extra_slice_header_bits（3 位）
    w.u(0, 1);    // sign_data_hiding_enabled_flag
    w.u(0, 1);    // cabac_init_present_flag
    w.ue(0);      // num_ref_idx_l0_default_active_minus1
    w.ue(0);      // num_ref_idx_l1_default_active_minus1
    w.se(0);      // init_qp_minus26
    w.u(0, 1);    // constrained_intra_pred_flag
    w.u(0, 1);    // transform_skip_enabled_flag
    w.u(0, 1);    // cu_qp_delta_enabled_flag
    w.se(0);      // pps_cb_qp_offset
    w.se(0);      // pps_cr_qp_offset
    w.u(0, 1);    // pps_slice_chroma_qp_offsets_present_flag
    w.u(0, 1);    // weighted_pred_flag
    w.u(0, 1);    // weighted_bipred_flag
    w.u(0, 1);    // transquant_bypass_enabled_flag
    w.u(0, 1);    // tiles_enabled_flag
    w.u(0, 1);    // entropy_coding_sync_enabled_flag
    w.u(0, 1);    // pps_loop_filter_across_slices_enabled_flag
    w.u(0, 1);    // deblocking_filter_control_present_flag（0→pps_deblocking_filter_disabled_flag 由解码器推断为 0）
    w.u(0, 1);    // pps_scaling_list_data_present_flag
    w.u(0, 1);    // lists_modification_present_flag
    w.ue(0);      // log2_parallel_merge_level_minus2
    w.u(0, 1);    // slice_segment_header_extension_present_flag
    w.u(0, 1);    // pps_extension_present_flag
    return w.finish();
}

// ── HEVC CABAC 段构造（H.265 标准算法，clean-room 实现）────────────────────
// 三张概率/状态表取自 H.265 标准（Table 9-37 / 9-38 / 9-39），属规范数据；
// 上下文初始化用 init_values + 标准公式（init_type = 2 - slice_type，I 帧 = 0）。
// 码流构造不移植任何解码器源码：它把每个 bin 的判定式写成对「段首 16 位
// CABAC 字节 B」的线性不等式，用仿射区间 [lo,hi) 求解析解，再用独立实现的
// 解码判定复算校验。行为与合规解码器逐位一致（本机 FFmpeg 9.0.2 全帧验证）。

// rangeTabLps：64 状态 × 4 个 range 区间（HEVC 用 9-bit range 模型，range∈[256,510]）。
static const uint8_t kLpsTable[64][4] = {
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
static const uint8_t kNextMPS[64] = {
    1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
    17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
    33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
    49,50,51,52,53,54,55,56,57,58,59,60,61,62,62,63
};
static const uint8_t kNextLPS[64] = {
    0,0,1,2,2,4,4,5,6,7,8,9,9,11,11,12,
    13,13,15,15,16,16,18,18,19,19,21,21,22,22,23,24,
    24,25,26,26,27,27,28,29,29,30,30,30,31,32,32,33,
    33,33,34,34,35,35,35,36,36,36,37,37,37,38,38,63
};

struct CtxModel { uint8_t state; };  // state = (pStateIdx << 1) | valMPS（HEVC 9.3.4.2 约定）

// HEVC CABAC 算术编码器（与 ff_hevc 解码器严格对偶的“比特级”模型，等价于 HEVC 9.3.4）。
// low：算术低位寄存器，有效位在 [9:0]（bit9=进位位，bit8=下一个待出比特）；range：9-bit (∈[256,510])。
// 每做一次 renorm：输出 (low>>8)&1 作为比特、用 (low>>9)&1 处理进位，再 low=(low<<1)&0x3FF、range<<=1。
// 注意：HEVC CABAC 启动时【不做】首比特反转（与 H.264 不同），输出比特原样写入码流。
// ── FFmpeg CABAC 解码器的精确对偶求解器 ────────────────────────────────────
// 背景：I_PCM 块之后 HEVC 要求重置算术引擎（9.3.4.4），FFmpeg 由
// hls_pcm_sample() 调 skip_bytes(&lc->cc, 96) 完成：它按 low 的低位回退
// bytestream 得到 PCM 起点，并从 ptr+96 重新 ff_init_cabac_decoder()。
// 解码端是 16-bit 优化模型（CABAC_BITS=16, low 为 26-bit 定点，range 为 9-bit），
// 与编码端 10-bit codILow 的写法不天然对齐；早期用 HM 式 FlushD 直接拼位流，
// 生成的 low 初值落在错误子区间，pcm_flag 恒被判 0（全帧退化成 intra 128）。
//
// 这里改为「按解码端模型反解」：每段固定输出 2 字节 CABAC（16 位 B），
// 解码端 ff_init_cabac_decoder 得到 low = B<<10 | (1<<9)（bytestream 偶地址支）
// 或 B<<10 | (b2<<2)+2（奇地址支）。把 low 视为 B 的仿射式 a*B + c，
// 则每个 bin 的 DecodeDecision / DecodeTerminate 判定都收缩出 B 的一个区间 [lo,hi)。
// 取区间下界即得正确 B；再用精确解码模拟（含两个地址分支）校验一遍。
// 这样编码端无需模拟进位/bitsToFollow，且与 FFmpeg 逐位一致（实测全帧精确）。
struct CabacEncoder {
    std::string final_out;    // [CU0 cabac 2B][CU0 raw 96B]...[end cabac][rbsp stop]
    CtxModel ctx[154];        // HEVC 共有 154 个上下文，跨段保持
    // 当前段的 bin 序列：first = ctxIdx（>=0 上下文 bin；-1 终止 bin），second = binVal
    std::vector<std::pair<int, int>> seg;

    // 一段最多 5 个 bin、每次 renorm <= 7 bit：a<=2^45、|c|<=2^61，
    // 标准 int64_t 足够，避免依赖 __int128 这类编译器扩展（MSVC 不支持）。
    using acc_t = std::int64_t;

    static acc_t ceil_div(acc_t p, acc_t q) {   // q > 0，数学意义上的 ceil
        acc_t d = p / q, r = p % q;             // C++ 向零截断；p<0 不整除时已等于 ceil
        if (r > 0) ++d;                      // p>0 不整除时需上取整
        return d;
    }
    // FFmpeg ff_h264_norm_shift[x]：x∈[0,511]，把 range 左移回 [256,510]
    static int norm_shift(int x) {
        if (x <= 0) return 9;
        int bits = 0;
        for (int t = x; t; t >>= 1) ++bits;
        int s = 9 - bits;
        return s > 0 ? s : 0;
    }
    // FFmpeg (ff_h264_mlps_state+128)[s] 的等价实现，s 为 state=(pStateIdx<<1)|valMPS
    static uint8_t next_state(int s, bool lps) {
        int p = (s >> 1) & 63, v = s & 1;
        if (!lps) return static_cast<uint8_t>((kNextMPS[p] << 1) | v);
        return static_cast<uint8_t>((kNextLPS[p] << 1) | (p == 0 ? 1 - v : v));
    }
    // FFmpeg ff_h264_lps_range[2*(range&0xC0)+s] == rangeTabLps[pStateIdx][(range>>6)&3]
    static int range_lps(int s, int range) {
        return kLpsTable[(s >> 1) & 63][(range >> 6) & 3];
    }

    // 上下文初始化（HEVC 9.3.1.1，与 ff_hevc_cabac_init 完全一致）。
    void init_contexts(int slice_qp) {
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
            ctx[idx].state = static_cast<uint8_t>(pre);
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

    // 登记一个 bin；段的实际码流在 flush_cu()/finish() 时统一反解。
    void encode_bin(int ctxIdx, int bin) { seg.push_back({ctxIdx, bin}); }

    // 终止 bin：pcm_flag / end_of_slice_segment_flag。
    // FFmpeg ff_hevc_pcm_flag_decode() = get_cabac_terminate()，不占上下文
    // （CABAC_ELEMS 中 PCM_FLAG 的 NUM_BINS = 0）。
    void encode_terminating(int bit) { seg.push_back({-1, bit}); }

    // 解析区间求解：返回可行 B 的下界（写入 lo/hi）。
    // low = a*B + c；ff_init_cabac_decoder 偶地址支给出 a=1024, c=512。
    // 每个 bin 的判定 T=(range-RangeLPS)<<17 对 B 产生 lo/hi 约束，renorm 时
    // low 与 range 同步左移（a、c 同步左移），故始终是仿射式，无需逐位模拟。
    // st 为本段起始的上下文状态快照；函数同时把 st 演化到段末（供下一 CU 段使用）。
    bool solve_bounds(acc_t& lo, acc_t& hi, uint8_t* st) const {
        lo = 0; hi = 65536;
        acc_t a = 1024, c = 512;
        int range = 510;
        for (const auto& e : seg) {
            const int ci = e.first, want = e.second;
            if (ci < 0) {                                  // DecodeTerminate
                range -= 2;
                const acc_t T = static_cast<acc_t>(range) << 17;
                if (want == 1) {                           // pcm_flag / 末尾 eos = 1
                    const acc_t lb = ceil_div(T - c, a);   // 需 low >= T
                    if (lb > lo) lo = lb;
                    break;                                 // 段结束，引擎随后重置
                }
                // end_of_slice_segment_flag = 0（非末尾 CTB）：需 low < T。
                // get_cabac_terminate 返回 0 时调 renorm_cabac_decoder_once()，
                // 引擎【不重置】，本段后续 bin（下一个 CTB 的 split）继续在同一引擎上。
                const acc_t ub = ceil_div(T - c, a);
                if (ub < hi) hi = ub;
                const int shift = (range < 256) ? 1 : 0;
                range <<= shift; a <<= shift; c <<= shift;
                if (lo >= hi) return false;
                continue;
            }
            const int s = st[ci];
            const int RangeLPS = range_lps(s, range);
            range -= RangeLPS;
            const acc_t T = static_cast<acc_t>(range) << 17;
            const bool need_lps = (want != (s & 1));
            if (need_lps) {
                const acc_t lb = ceil_div(T - c, a);
                if (lb > lo) lo = lb;
                c -= T;
                range = RangeLPS;
            } else {
                const acc_t ub = ceil_div(T - c, a);       // MPS 需 low < T
                if (ub < hi) hi = ub;
            }
            st[ci] = next_state(s, need_lps);
            const int shift = norm_shift(range);
            range <<= shift;
            a <<= shift;
            c <<= shift;
            if (lo >= hi) return false;
        }
        return lo < hi;
    }

    // 精确复刻 FFmpeg get_cabac_inline / get_cabac_terminate / skip_bytes 的校验。
    // odd_addr 覆盖 ff_init_cabac_decoder 按 bytestream 地址奇偶分支读 2/3 字节的两种情形。
    bool verify(uint32_t B, bool odd_addr, uint8_t raw0, const uint8_t* st0,
                bool check_ptr) const {
        uint8_t st[154];
        for (int i = 0; i < 154; ++i) st[i] = st0[i];
        const uint8_t d0 = static_cast<uint8_t>(B >> 8), d1 = static_cast<uint8_t>(B);
        // 用 uint32_t 承载 low，规避 FFmpeg 原始实现里 int 左移的有符号溢出 UB。
        uint32_t low = (static_cast<uint32_t>(d0) << 18) +
                       (static_cast<uint32_t>(d1) << 10);
        int bytestream;
        if (!odd_addr) { low += (1u << 9); bytestream = 2; }
        else { low += (static_cast<uint32_t>(raw0) << 2) + 2u; bytestream = 3; }
        int range = 0x1FE;
        if ((static_cast<uint32_t>(range) << 17) < low) return false;  // AVERROR_INVALIDDATA
        for (const auto& e : seg) {
            const int ci = e.first, want = e.second;
            if (ci < 0) {
                range -= 2;
                const uint32_t T = static_cast<uint32_t>(range) << 17;
                if (want == 1) {
                    if (low < T) return false;             // pcm_flag 会被判 0
                    break;
                }
                if (low >= T) return false;                // eos=0 需要 low < T
                const int shift = (range < 256) ? 1 : 0;  // renorm_cabac_decoder_once
                range <<= shift; low <<= shift;
                if (!(low & 0xFFFF)) return false;
                continue;
            }
            const int s = st[ci];
            const int RangeLPS = range_lps(s, range);
            range -= RangeLPS;
            const uint32_t T = static_cast<uint32_t>(range) << 17;
            const bool lps = (low > T);      // ((T - low) >> 31) != 0 的无符号等价
            const int bit = lps ? (1 - (s & 1)) : (s & 1);
            if (bit != want) return false;
            if (lps) { low -= T; range = RangeLPS; }
            st[ci] = next_state(s, lps);
            const int shift = norm_shift(range);
            range <<= shift;
            low <<= shift;
            // refill2 会多读 2 字节，使 skip_bytes 的 ptr 整体偏 2、PCM 起点错位；
            // 因此本段的合法 B 必须保证段内不触发 refill2。
            if (!(low & 0xFFFF)) return false;
        }
        if (check_ptr) {
            int ptr = bytestream;
            if (low & 0x1) --ptr;
            if (low & 0x1FF) --ptr;                        // CABAC_BITS == 16
            if (ptr != 2) return false;                    // PCM 必须紧跟 2 字节 CABAC
        }
        return true;
    }

    // 求解本段 2 字节 CABAC：解析区间取下界，精确解码模拟在两个地址分支校验。
    // 若区间下界不满足（例如本段 refill2 触发使仿射假设失效），顺延扫描整个区间。
    uint32_t solve(uint8_t raw0, bool check_ptr) {
        uint8_t st0[154];
        for (int i = 0; i < 154; ++i) st0[i] = ctx[i].state;
        acc_t lo, hi;
        uint8_t evolved[154];
        for (int i = 0; i < 154; ++i) evolved[i] = st0[i];
        const bool ok = solve_bounds(lo, hi, evolved);
        if (ok) {
            for (acc_t v = lo; v < hi && v <= 0xFFFF; ++v) {
                const uint32_t B = static_cast<uint32_t>(v);
                if (verify(B, false, raw0, st0, check_ptr) &&
                    verify(B, true, raw0, st0, check_ptr)) {
                    for (int i = 0; i < 154; ++i) ctx[i].state = evolved[i];
                    return B;
                }
            }
        }
        // 兜底：全空间扫描（解析解只在 refill2 未触发时成立）
        for (uint32_t B = 0; B <= 0xFFFF; ++B) {
            if (verify(B, false, raw0, st0, check_ptr) &&
                verify(B, true, raw0, st0, check_ptr)) {
                uint8_t tmp[154];
                for (int i = 0; i < 154; ++i) tmp[i] = ctx[i].state;
                for (const auto& e : seg) {
                    if (e.first >= 0)
                        tmp[e.first] = next_state(tmp[e.first],
                                                  e.second != (tmp[e.first] & 1));
                }
                for (int i = 0; i < 154; ++i) ctx[i].state = tmp[i];
                return B;
            }
        }
        return 0;
    }

    void flush_cu(std::string raw) {
        const uint32_t B = solve(static_cast<uint8_t>(raw[0]), true);
        final_out.push_back(static_cast<char>(B >> 8));
        final_out.push_back(static_cast<char>(B & 0xFF));
        final_out += raw;                  // pcm_sample_luma/chroma，字节对齐
        // HEVC 9.3.4.4：PCM 后算术引擎重置（每段各自 ff_init_cabac_decoder）；
        // 上下文模型 ctx[] 跨段保持，solve() 已把它演化到段末状态。
        seg.clear();
    }

    // 切片末尾：末段（end_of_slice_flag 终止 bin）+ rbsp_stop_one_bit(1) + 补零。
    // 末段之后没有 PCM 数据，skip_bytes 不参与，故不检查 ptr。
    std::string finish() {
        const uint32_t B = solve(0, false);
        final_out.push_back(static_cast<char>(B >> 8));
        final_out.push_back(static_cast<char>(B & 0xFF));
        final_out.push_back(static_cast<char>(0x80));   // stop bit 1 + 7 个 0
        seg.clear();
        return std::move(final_out);
    }
};

// 编码树：log2size > 3（>8x8）写 split_cu_flag=1 并四等分；到 8x8 叶写 PCM 块。
// split_cu_flag 的上下文按 HEVC spec 9.3.4.2 取：CTB 根（log2size==6）恒为 ctxIdx 0
// （ctxInc=0）；其余深度 ctxInc = condL + condA。本码流是均匀四分树，每个非根 CU 的
// 左/上邻居必为同 CTB 内已切分的 CU，故 condL = condA = 1 → ctxIdx = 2（含 CTB 左/上
// 边界，规范规定 x0==xCtb 即 condL=1，不能因 px==0 算成 0）。
void code_cu(CabacEncoder& cabac, const Yuv420& f, int x0, int y0, int log2size,
             int ctb_x, int ctb_y) {
    const int size = 1 << log2size;
    if (log2size > 3) {  // MinCbLog2SizeY = 3（8x8）
        // split_cu_flag 的 ctxInc 严格按 HEVC 9.3.4.2.2 推导：
        //   · 仅当本 CU 是 CTB 根（64×64，log2size==6）时 ctxInc = condL + condA
        //     = (ctb_x>0) + (ctb_y>0)；左上角 CTB 即为 0。
        //   · 非根 CU 位于 CTB 上/左边界（x0==ctb_x && y0==ctb_y，即 CTB 内左上对角子块）
        //     时 ctxInc = 1（此时 condL=condA=0 → FFMIN(2,1+0+0)=1）。
        //   · 其余内部子块 ctxInc = 2（FFMIN(2,1+condL+condA)，condL/condA 至少一个为 1）。
        //   之前把「CTB 内左上对角子块（如 32×32/16×16 的 (0,0)）」也错当成根写成 ctx=0，
        //   导致自第一个子块起 CABAC 上下文状态与解码器（FFmpeg）错位，叶片 pcm_flag 被误读 → 全帧当 intra。
        // FFmpeg ff_hevc_split_coding_unit_flag_decode():
        //   int x0b = av_zero_extend(x0, log2_ctb_size);   // CTB 内偏移
        //   if (lc->ctb_left_flag || x0b) depth_left = tab_ct_depth[...x_cb-1];
        //   if (lc->ctb_up_flag   || y0b) depth_top  = tab_ct_depth[...y_cb-1];
        //   inc = (depth_left > ct_depth) + (depth_top > ct_depth)
        // 均匀四分树全拆到 8x8 叶子时，已编码叶子的 ct_depth 恒为 3，
        // 而待判 split 的 ct_depth 为 0/1/2，故只要「左侧有已编码 CU」即 inc+1。
        // 「左侧有已编码 CU」= 同 CTB 内 x0b>0 或存在左侧 CTB，两者合起来正是绝对坐标 x0>0；
        // 用 CTB 内坐标会在第 2 个及以后的 CTB 左/上边界漏掉邻居，导致右半帧全错。
        const int ctx = (x0 > 0 ? 1 : 0) + (y0 > 0 ? 1 : 0);
        cabac.encode_bin(ctx, 1);  // split_cu_flag
        const int half = size / 2;
        for (int i = 0; i < 4; ++i) {
            const int x1 = x0 + (i & 1) * half;
            const int y1 = y0 + (i >> 1) * half;
            code_cu(cabac, f, x1, y1, log2size - 1, ctb_x, ctb_y);
        }
        return;
    }
    // 8x8 叶（== MinCbLog2SizeY）：HEVC 7.3.8.5 coding_unit() 语法顺序要求先写 part_mode，
    // 当且仅当 part_mode == PART_2Nx2N 且满足 PCM 条件时才读 pcm_flag。8x8 恰好等于 MinCbLog2SizeY，
    // 故 part_mode 必须显式写出（不可省略）；只有 log2CbSize > MinCbLog2SizeY 时 PART_2Nx2N 才是
    // 唯一合法值、可由 split_cu_flag 隐式决定而省略。
    // part_mode 首 bin：FFmpeg ff_hevc_part_mode_decode() 的
    //   if (GET_CABAC(PART_MODE_OFFSET)) return PART_2Nx2N;
    // 即 bin "1" → PART_2Nx2N，只读 1 个 bin。PART_MODE_OFFSET = 13。
    cabac.encode_bin(13, 1);
    // pcm_flag 是【终止 bin】：FFmpeg ff_hevc_pcm_flag_decode() 直接返回
    // get_cabac_terminate(&lc->cc)，不占用任何上下文（PCM_FLAG 的 NUM_BINS=0）。
    cabac.encode_terminating(1);
    // 字节对齐（pcm_alignment_zero_bit）后写裸样本，落盘到 final_out。
    std::string raw; raw.reserve(96);
    const int cstride = f.cw / 2;
    for (int yy = 0; yy < 8; ++yy)  // luma 8x8
        for (int xx = 0; xx < 8; ++xx)
            raw.push_back(f.y[static_cast<size_t>((y0 + yy) * f.cw + (x0 + xx))]);
    for (int yy = 0; yy < 4; ++yy)  // Cb 4x4
        for (int xx = 0; xx < 4; ++xx)
            raw.push_back(f.u[static_cast<size_t>(((y0 / 2) + yy) * cstride + ((x0 / 2) + xx))]);
    for (int yy = 0; yy < 4; ++yy)  // Cr 4x4
        for (int xx = 0; xx < 4; ++xx)
            raw.push_back(f.v[static_cast<size_t>(((y0 / 2) + yy) * cstride + ((x0 / 2) + xx))]);
    cabac.flush_cu(std::move(raw));  // 字节对齐 + 拼 [cabac][raw] 到 final_out
}

std::string build_idr_slice(const Yuv420& f, int slice_qp) {
    BitWriter w;
    // HEVC 7.3.6.1 slice_segment_header()（first_slice_segment_in_pic_flag=1，
    // pps.dependent_slice_segments_enabled_flag=0，非 dependent 分支）：
    w.u(1, 1);    // first_slice_segment_in_pic_flag
    // IDR/BLA/CRA（nal_unit_type ∈ [BLA_W_LP, RSV_IRAP_VCL23]，本流为 IDR_W_RADL=19）
    // 首片必须读 no_output_of_prior_pics_flag（HEVC 7.3.6.1；FFmpeg 9.0.2 实测会读）。
    w.u(0, 1);    // no_output_of_prior_pics_flag
    w.ue(0);      // slice_pic_parameter_set_id
    // 本 PPS：num_extra_slice_header_bits=0 ⇒ 0 个 slice_reserved_flag；output_flag_present_flag=0
    // ⇒ 无 pic_output_flag；separate_colour_plane_flag=0 ⇒ 无 colour_plane_id。
    w.ue(2);      // slice_type = 2 (I)
    // 关键：nal_unit_type == IDR_W_RADL ⇒ HEVC 7.3.6.1 不读 slice_pic_order_cnt_lsb 与
    // short_term_ref_pic_set（IDR 无参考帧）。早期版本多写这 4 bit 会把后续所有字段整体后移，
    // 最终 byte_alignment 处的 alignment_bit 落到填充 0 上 → "alignment_bit_equal_to_one=0"，
    // FFmpeg 直接跳过整个 slice NALU。故此处【绝不】写 poc_lsb。
    // 同样，本 PPS：tiles_enabled_flag=0 且 entropy_coding_sync_enabled_flag=0 ⇒ 不读
    // num_entry_point_offsets；pps_loop_filter_across_slices_enabled_flag=0 ⇒
    // slice_loop_filter_across_slices_enabled_flag 由解码端推断（不读）；
    // sample_adaptive_offset_enabled_flag=0 ⇒ 无 slice_sao_*_flag；
    // cabac_init_present_flag=0 ⇒ 无 cabac_init_flag；
    // deblocking_filter_control_present_flag=0 ⇒ 无 slice_deblocking_filter_disabled_flag；
    // pps_slice_chroma_qp_offsets_present_flag=0 ⇒ 无 slice_cb/cr_qp_offset。
    // 因此 slice_type 之后本配置下解码端实际只再读一个字段：
    w.se(0);      // slice_qp_delta（init_qp_minus26=0 ⇒ 0）
    // HEVC 7.3.6.1：slice_segment_header() 末尾必须有 byte_alignment()（先写 1 作为
    // alignment_bit_equal_to_one，再补 0 到字节边界），随后 CABAC 从该字节边界开始。
    // 必须用 w.finish()（先 put 1 再补零）把整个头 flush 成整数字节。
    std::string rbsp = w.finish();
    CabacEncoder cabac;
    cabac.init_contexts(slice_qp);
    bool first_ctb = true;
    for (int ctb_y = 0; ctb_y < f.ch; ctb_y += 64) {
        for (int ctb_x = 0; ctb_x < f.cw; ctb_x += 64) {
            // HEVC 7.3.8.1：每个 CTB 的 coding_tree_unit() 之后都读一次
            // end_of_slice_segment_flag。除最后一个 CTB 外必须为 0，否则解码器
            // 会提前结束 slice，剩余 CTB 全部丢失（表现为右半/下半帧全错）。
            // 该 bin 属于【重置后】的新 CABAC 引擎，即下一个 CTB 首个 CU 段的段首。
            if (!first_ctb) cabac.encode_terminating(0);
            first_ctb = false;
            code_cu(cabac, f, ctb_x, ctb_y, 6, ctb_x, ctb_y);
        }
    }
    cabac.encode_terminating(1);  // 最后一个 CTB 的 end_of_slice_segment_flag = 1
    rbsp += cabac.finish();
    return rbsp;
}

// I_PCM 档：完整自包含实现。后续把 bit_writer / parameter_sets / cabac_* /
// ctb_tree 等下沉到 image/codec/hevc/ 时，只服务真压缩档；这条 PCM 路径保持
// 原样，作为 diag_hevc.py 的字节级黄金回归基线，任何一步都不许改变其码流。
std::string encode_hevc_pcm_annexb(std::span<const Bitmap> frames) {
    if (frames.empty()) return std::string{};
    const int W = static_cast<int>(frames[0].width());
    const int H = static_cast<int>(frames[0].height());
    for (const auto& f : frames) {
        if (static_cast<int>(f.width()) != W || static_cast<int>(f.height()) != H) {
            return std::string{};  // 暂要求统一尺寸
        }
    }
    const int cw = ((W + 63) / 64) * 64;
    const int ch = ((H + 63) / 64) * 64;

    std::string out;
    out += make_nal(32, build_vps());  // VPS
    out += make_nal(33, build_sps(cw, ch));  // SPS
    out += make_nal(34, build_pps());  // PPS
    for (const auto& frame : frames) {
        const Yuv420 f = to_yuv(frame);
        out += make_nal(19, build_idr_slice(f, 26));  // IDR slice, slice QP = 26
    }
    return out;
}

}  // namespace

std::string encode_hevc_annexb(std::span<const Bitmap> frames, const EncodeOptions& opts) {
    switch (opts.video_mode) {
        case VideoEncodeMode::Pcm:
            return encode_hevc_pcm_annexb(frames);
        case VideoEncodeMode::Intra:
        case VideoEncodeMode::Inter:
            // 阶段一/二才落地；显式返回空串，不静默降级成 PCM。
            return std::string{};
    }
    return std::string{};
}

}  // namespace termchart::image
