// hevc_encode.cpp —— 自写 HEVC（H.265, Main profile）编码器（I_PCM 档）
#include "termchart/image/hevc_encode.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace termchart::image {
namespace {

// ── MSB-first 位写入器（HEVC 高位在前）──
struct BitWriter {
    std::string buf;
    int cur = 0;
    int nbits = 0;
    void put_bit(int b) {
        cur = (cur << 1) | (b & 1);
        if (++nbits == 8) {
            buf.push_back(static_cast<char>(cur));
            cur = 0;
            nbits = 0;
        }
    }
    void u(unsigned long long value, int len) {
        for (int i = len - 1; i >= 0; --i) {
            put_bit(static_cast<int>((value >> i) & 1ull));
        }
    }
    void ue(unsigned v) {
        unsigned code = v + 1;
        int n = 0;
        for (unsigned t = code; t > 1; t >>= 1) ++n;
        for (int i = 0; i < n; ++i) put_bit(0);
        for (int i = n; i >= 0; --i) put_bit(static_cast<int>((code >> i) & 1u));
    }
    void se(int v) {
        unsigned k = (v <= 0) ? static_cast<unsigned>(-2 * v)
                               : static_cast<unsigned>(2 * v - 1);
        ue(k);
    }
    // 仅补零对齐到字节边界（PCM 的 pcm_alignment_zero_bit / 各层 byte alignment 用）。
    void align_zero() {
        while (nbits != 0) put_bit(0);
    }
    // rbsp_trailing_bits：先写停止位 1 再补零对齐，用于 NAL / SPS/PPS/VPS / slice 末尾。
    std::string finish() {
        put_bit(1);
        align_zero();
        return std::move(buf);
    }
};

// 防竞争字节：在 00 00 后且下个字节 ≤3 时插入 0x03（与 H.264 同规则）。
std::string add_ep(const std::string& rbsp) {
    std::string out;
    int zeros = 0;
    for (char ch : rbsp) {
        const int c = static_cast<unsigned char>(ch);

        if (zeros >= 2 && c <= 3) {
            out.push_back(static_cast<char>(3));
            zeros = 0;
        }
        out.push_back(ch);
        zeros = (c == 0) ? zeros + 1 : 0;
    }
    return out;
}

// HEVC NAL：2 字节头（forbidden_zero(1) | nal_unit_type(6) | nuh_layer_id(6) |
// nuh_temporal_id_plus1(3)） + EP(RBSP)，前面补 00 00 00 01 起始码。
std::string make_nal(int type, const std::string& rbsp) {
    std::string ep = add_ep(rbsp);
    std::string out;
    out.push_back(static_cast<char>(0));
    out.push_back(static_cast<char>(0));
    out.push_back(static_cast<char>(0));
    out.push_back(static_cast<char>(1));  // start code 00 00 00 01
    const unsigned header = static_cast<unsigned>(type) << 9 | 1u;  // layer=0, temporal=1
    out.push_back(static_cast<char>((header >> 8) & 0xFF));
    out.push_back(static_cast<char>(header & 0xFF));
    out += ep;
    return out;
}

int clamp8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

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
// 注意：Main profile（idc=1）在 4 个基础约束标志之后，FFmpeg 的 decode_profile_tier_level
// 还会读 9 个 Main 专属约束标志 + 34 reserved + inbld_flag，共 48 位约束区（不是 43/44）。
void profile_tier_level(BitWriter& w) {
    w.u(0, 2);   // general_profile_space
    w.u(0, 1);   // general_tier_flag
    w.u(1, 5);   // general_profile_idc = 1 (Main, 8-bit)
    w.u(1u << 1, 32);  // general_profile_compatibility_flags（第 1 位 = Main）
    w.u(1, 1);   // general_progressive_source_flag
    w.u(0, 1);   // general_interlaced_source_flag
    w.u(1, 1);   // general_non_packed_constraint_flag
    w.u(1, 1);   // general_frame_only_constraint_flag
    // Main profile 专属约束标志（与 FFmpeg decode_profile_tier_level 的 idc=1 分支一致）：
    w.u(0, 1);   // max_12bit_constraint_flag
    w.u(0, 1);   // max_10bit_constraint_flag
    w.u(1, 1);   // max_8bit_constraint_flag
    w.u(0, 1);   // max_422chroma_constraint_flag
    w.u(1, 1);   // max_420chroma_constraint_flag
    w.u(0, 1);   // max_monochrome_constraint_flag
    w.u(0, 1);   // intra_constraint_flag
    w.u(0, 1);   // one_picture_only_constraint_flag
    w.u(0, 1);   // lower_bit_rate_constraint_flag
    w.u(0, 34);  // XXX_reserved_zero_34bits
    w.u(0, 1);   // inbld_flag（check_profile_idc(1) 分支）
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
    w.u(1, 1);    // deblocking_filter_control_present_flag
    w.u(0, 1);    // deblocking_filter_override_enabled_flag
    w.u(1, 1);    // pps_deblocking_filter_disabled_flag
    w.u(0, 1);    // pps_scaling_list_data_present_flag
    w.u(0, 1);    // lists_modification_present_flag
    w.ue(0);      // log2_parallel_merge_level_minus2
    w.u(0, 1);    // slice_segment_header_extension_present_flag
    w.u(0, 1);    // pps_extension_present_flag
    return w.finish();
}

// 递归 coding tree：块尺寸 s（以 log2 表示）> 8 时写 split_cu_flag=1 并四等分；
// 到 8x8 叶则写 PCM 块（luma 8x8 + Cb 4x4 + Cr 4x4，各 8 bit）。
void code_cu(BitWriter& w, const Yuv420& f, int x0, int y0, int log2size) {
    const int size = 1 << log2size;
    if (log2size > 3) {  // MinCbLog2SizeY = 3（8x8）
        w.u(1, 1);  // split_cu_flag
        const int half = size / 2;
        for (int i = 0; i < 4; ++i) {
            const int x1 = x0 + (i & 1) * half;
            const int y1 = y0 + (i >> 1) * half;
            code_cu(w, f, x1, y1, log2size - 1);
        }
        return;
    }
    // 8x8 叶：cu_transquant_bypass_flag 不出现（PPS 关）→ pcm 允许 → pcm_flag=1
    w.u(1, 1);        // pcm_flag
    w.align_zero();    // pcm_alignment_zero_bit
    const int cstride = f.cw / 2;
    for (int yy = 0; yy < 8; ++yy)  // luma 8x8
        for (int xx = 0; xx < 8; ++xx)
            w.u(f.y[static_cast<size_t>((y0 + yy) * f.cw + (x0 + xx))], 8);
    for (int yy = 0; yy < 4; ++yy)  // Cb 4x4
        for (int xx = 0; xx < 4; ++xx)
            w.u(f.u[static_cast<size_t>(((y0 / 2) + yy) * cstride + ((x0 / 2) + xx))], 8);
    for (int yy = 0; yy < 4; ++yy)  // Cr 4x4
        for (int xx = 0; xx < 4; ++xx)
            w.u(f.v[static_cast<size_t>(((y0 / 2) + yy) * cstride + ((x0 / 2) + xx))], 8);
}

std::string build_idr_slice(const Yuv420& f) {
    BitWriter w;
    w.u(1, 1);    // first_slice_segment_in_pic_flag
    w.u(0, 1);    // no_output_of_prior_pics_flag（IDR 属 IRAP，必须读 1 位）
    w.ue(0);      // slice_pic_parameter_set_id
    w.ue(2);      // slice_type = 2 (I)
    w.se(0);      // slice_qp_delta（se）
    // 之后直接进入 coding tree：逐 CTB（64x64）递归。
    // 说明：output_flag / SAO / POC-RPS / 参考列表 / 色度 qp / deblocking override /
    // 跨片环路滤波 / entry points / slice header extension 等字段，因 PPS/SPS 对应
    // 标志均为 0 或 override=0，FFmpeg 在此全部跳过，无需写位。
    const int ctb_log2 = 6;  // CTB = 64
    for (int ctb_y = 0; ctb_y < f.ch; ctb_y += 64)
        for (int ctb_x = 0; ctb_x < f.cw; ctb_x += 64)
            code_cu(w, f, ctb_x, ctb_y, ctb_log2);
    return w.finish();
}

}  // namespace

std::string encode_hevc_annexb(std::span<const Bitmap> frames, const EncodeOptions&) {
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
        out += make_nal(19, build_idr_slice(f));  // IDR slice
    }
    return out;
}

}  // namespace termchart::image
