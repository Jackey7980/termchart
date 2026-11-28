// h264_encode.cpp —— 自写 H.264（baseline AVC）编码器（I_PCM 档）
#include "termchart/media/image/h264_encode.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "h264_bits.hpp"
#include "h264_intra.hpp"

namespace termchart::image {
namespace {

// BitWriter / 防竞争 / NAL 封装统一来自 h264_bits.hpp（底层是
// codec/bitstream/bit_writer.hpp，与 HEVC 共用），此处不再本地重写。
using h264_detail::add_ep;
using h264_detail::BitWriter;
using h264_detail::make_nal;

int clamp8(int v) { return h264_detail::clamp8(v); }

// 单帧 RGB(A) → YUV420（BT.601），并按 16 对齐补边；返回 Y/U/V 三块与编码尺寸。
struct Yuv420 {
    int cw = 0, ch = 0;
    std::vector<uint8_t> y, u, v;
};

Yuv420 to_yuv(const Bitmap& bmp) {
    const int W = static_cast<int>(bmp.width());
    const int H = static_cast<int>(bmp.height());
    const int cw = ((W + 15) / 16) * 16;
    const int ch = ((H + 15) / 16) * 16;

    const size_t cw_s = static_cast<size_t>(cw);
    const size_t ch_s = static_cast<size_t>(ch);
    const size_t hcw_s = cw_s / 2;  // 半宽
    const size_t hch_s = ch_s / 2;  // 半高

    Yuv420 out;
    out.cw = cw;
    out.ch = ch;
    out.y.assign(cw_s * ch_s, 0);
    out.u.assign(hcw_s * hch_s, 128);
    out.v.assign(hcw_s * hch_s, 128);

    for (int yy = 0; yy < H; ++yy) {
        for (int xx = 0; xx < W; ++xx) {
            const int r = bmp.at(static_cast<uint32_t>(xx), static_cast<uint32_t>(yy), 0);
            const int g = bmp.at(static_cast<uint32_t>(xx), static_cast<uint32_t>(yy), 1);
            const int bb = bmp.at(static_cast<uint32_t>(xx), static_cast<uint32_t>(yy), 2);
            const int ly = clamp8(((66 * r + 129 * g + 25 * bb + 128) >> 8) + 16);
            out.y[static_cast<size_t>(yy) * cw_s + static_cast<size_t>(xx)] = static_cast<uint8_t>(ly);
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

std::string build_sps(int cw, int ch) {
    BitWriter w;
    w.u(66, 8);                 // profile_idc = Baseline
    w.u((1 << 7), 8);           // constraint_set0_flag=1，其余 0（= 0x80）
    w.u(30, 8);                 // level_idc
    w.ue(0);                    // seq_parameter_set_id
    // 注意：chroma_format_idc / bit_depth / seq_scaling_matrix 等字段仅 high profile
    // （profile_idc∈{100,110,122,...}）才出现在 SPS 里；Baseline(66) 推断 chroma=4:2:0、
    // bit_depth=8，故此处直接跳到 log2_max_frame_num。多写会令后续字段整体错位。
    w.ue(0);                    // log2_max_frame_num_minus4
    w.ue(0);                    // pic_order_cnt_type
    w.ue(0);                    // log2_max_pic_order_cnt_lsb_minus4
    w.ue(1);                    // max_num_ref_frames
    w.u(0, 1);                  // gaps_in_frame_num_value_allowed_flag
    w.ue(static_cast<unsigned>(cw) / 16 - 1);  // pic_width_in_mbs_minus1
    w.ue(static_cast<unsigned>(ch) / 16 - 1);  // pic_height_in_map_units_minus1
    w.u(1, 1);                  // frame_mbs_only_flag
    w.u(0, 1);                  // direct_8x8_inference_flag（始终存在，非 frame_mbs_only 时才省）
    w.u(0, 1);                  // frame_cropping_flag = 0（v1 不裁剪）
    w.u(0, 1);                  // vui_parameters_present_flag
    return w.finish();
}

std::string build_pps() {
    BitWriter w;
    w.ue(0);                    // pic_parameter_set_id
    w.ue(0);                    // seq_parameter_set_id
    w.u(0, 1);                  // entropy_coding_mode_flag = 0 (CAVLC)
    w.u(0, 1);                  // bottom_field_pic_order_in_frame_present_flag
    w.ue(0);                    // num_slice_groups_minus1
    w.ue(0);                    // num_ref_idx_l0_default_active_minus1
    w.ue(0);                    // num_ref_idx_l1_default_active_minus1
    w.u(0, 1);                  // weighted_pred_flag
    w.u(0, 2);                  // weighted_bipred_idc
    w.se(0);                    // pic_init_qp_minus26
    w.se(0);                    // pic_init_qs_minus26
    w.se(0);                    // chroma_qp_index_offset
    w.u(0, 1);                  // deblocking_filter_control_present_flag
    w.u(0, 1);                  // constrained_intra_pred_flag
    w.u(0, 1);                  // redundant_pic_cnt_present_flag
    // 关键：这三个字段属于 PPS 的「more_rbsp_data() 分支」。若不显式写出来，
    // 解码器（FFmpeg h264_ps.c）看到标志位之后仍有剩余位（我们补的 stop bit + 对齐零），
    // 就会继续读，把 rbsp_stop_one_bit 当成 transform_8x8_mode_flag = 1。
    // 一旦被误判成 1，I_NxN 宏块在 mb_type 之后会多读 1 bit 的 transform_size_8x8_flag，
    // 导致后续所有模式/残差整体错位（I_PCM 档不读这个 flag，所以以前没暴露）。
    w.u(0, 1);                  // transform_8x8_mode_flag = 0
    w.u(0, 1);                  // pic_scaling_matrix_present_flag
    w.se(0);                    // second_chroma_qp_index_offset
    return w.finish();
}

// 一帧 I_PCM 的 IDR slice（mb_type=25，直接吐原始采样）。
std::string build_idr_slice(const Yuv420& f, int idr_id) {
    BitWriter w;
    w.ue(0);                    // first_mb_in_slice
    w.ue(2);                    // slice_type = I（参考型；IDR 要求 nal_ref_idc!=0，须用 2 而非 7）
    w.ue(0);                    // pic_parameter_set_id
    w.u(0, 4);                  // frame_num
    w.ue(static_cast<unsigned>(idr_id));  // idr_pic_id
    w.u(0, 4);                  // pic_order_cnt_lsb
    // dec_ref_pic_marking（IDR 专属，必须在 slice_qp_delta 之前）
    w.u(0, 1);                  // no_output_of_prior_pics_flag
    w.u(0, 1);                  // long_term_reference_flag
    w.se(0);                    // slice_qp_delta（dec_ref_pic_marking 之后）

    const int mbs_x = f.cw / 16;
    const int mbs_y = f.ch / 16;
    const int cstride = f.cw / 2;

    const size_t cw_s = static_cast<size_t>(f.cw);
    const size_t cstride_s = static_cast<size_t>(cstride);

    for (int mby = 0; mby < mbs_y; ++mby) {
        for (int mbx = 0; mbx < mbs_x; ++mbx) {
            w.ue(25);           // mb_type = I_PCM
            w.align_zero();      // pcm_alignment_zero_bit（只补零，不可带停止位）
            // 16x16 亮度，光栅序
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    const size_t idx =
                        static_cast<size_t>(mby * 16 + y) * cw_s + static_cast<size_t>(mbx * 16 + x);
                    w.u(f.y[idx], 8);
                }
            }
            // 8x8 Cb
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    const size_t idx =
                        static_cast<size_t>(mby * 8 + y) * cstride_s + static_cast<size_t>(mbx * 8 + x);
                    w.u(f.u[idx], 8);
                }
            }
            // 8x8 Cr
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    const size_t idx =
                        static_cast<size_t>(mby * 8 + y) * cstride_s + static_cast<size_t>(mbx * 8 + x);
                    w.u(f.v[idx], 8);
                }
            }
        }
    }
    return w.finish();
}

// Yuv420（本文件）与 h264_detail::YuvFrame（帧内档）字段完全同构，直接搬。
h264_detail::YuvFrame to_intra_frame(const Yuv420& f) {
    h264_detail::YuvFrame out;
    out.cw = f.cw;
    out.ch = f.ch;
    out.y = f.y;
    out.u = f.u;
    out.v = f.v;
    return out;
}

}  // namespace

std::string encode_h264_annexb(std::span<const Bitmap> frames, const EncodeOptions& opts) {
    if (frames.empty()) return std::string{};
    int W = static_cast<int>(frames[0].width());
    int H = static_cast<int>(frames[0].height());
    for (const auto& f : frames) {
        if (static_cast<int>(f.width()) != W || static_cast<int>(f.height()) != H) {
            return std::string{};  // 暂要求统一尺寸
        }
    }
    const int cw = ((W + 15) / 16) * 16;
    const int ch = ((H + 15) / 16) * 16;

    std::string out;
    out += make_nal(3, 7, build_sps(cw, ch));   // SPS
    out += make_nal(3, 8, build_pps());         // PPS
    // 统一走 EncodeOptions::video_mode；保留旧环境变量作为向后兼容的对照开关。
    // 默认 Pcm 仍是 I_PCM，两档共用同一套 SPS/PPS 与 NAL 封装。
    const bool use_intra = opts.video_mode != VideoEncodeMode::Pcm ||
                           std::getenv("TERMCHART_H264_INTRA") != nullptr;
    for (size_t i = 0; i < frames.size(); ++i) {
        const Yuv420 f = to_yuv(frames[i]);
        const std::string rbsp =
            use_intra ? h264_detail::build_idr_slice_intra(to_intra_frame(f), static_cast<int>(i), 26)
                      : build_idr_slice(f, static_cast<int>(i));
        out += make_nal(3, 5, rbsp);  // IDR slice
    }
    return out;
}

}  // namespace termchart::image
