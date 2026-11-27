// av1_inter_probe.cpp —— 阶段五探针：两帧 8x8 IVF，KEY + INTER(GLOBALMV)。
// 用法: av1_inter_probe <mv_x_q3> <mv_y_q3> <out.ivf> <out.yuv> [newmv_probe]
//                       [interp_filter 0..3] [with_residual] [gmv_translation]
//                       ... [qm_level 0..15]（argv[24]，15=不用 qmatrix）
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "codec/av1/bitstream/headers.hpp"
#include "codec/av1/bitstream/ivf_muxer.hpp"
#include "codec/av1/bitstream/obu_header.hpp"
#include "codec/av1/bitstream/tile_group.hpp"
#include "codec/av1/entropy/coef_cdf_data.hpp"

using namespace termchart::image::codec::av1;

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr, "usage: av1_inter_probe mvx mvy ivf yuv\n");
        return 2;
    }
    const int mv_x = std::atoi(argv[1]);
    const int mv_y = std::atoi(argv[2]);
    const int newmv_probe = argc > 5 ? std::atoi(argv[5]) : -1;
    const int filter_idx = argc > 6 ? std::atoi(argv[6]) & 3 : 0;
    const InterpFilter filter = filter_idx == 1   ? InterpFilter::Smooth
                                : filter_idx == 2 ? InterpFilter::Sharp
                                : filter_idx == 3 ? InterpFilter::Bilinear
                                                  : InterpFilter::Regular;

    SequenceHeaderConfig seq;
    seq.width = 8;
    seq.height = 8;
    seq.inter_frames = true;
    FrameHeaderConfig key;
    key.frame_type = 0;
    key.base_q_idx = 100;
    FrameHeaderConfig inter;
    inter.frame_type = 1;
    inter.base_q_idx = 100;
    inter.global_mv_x = mv_x;
    inter.global_mv_y = mv_y;
    inter.interp_filter = filter_idx;

    const int with_residual = argc > 7 ? std::atoi(argv[7]) : 0;
    // argv[8]：1 时帧头写 TRANSLATION 全局运动（gm 平移量直接取 mv_x/mv_y），
    // 块仍走 GLOBALMV —— 于是块的 MV 完全来自帧级 GMV 矩阵而不是 MVD 差分。
    const int gmv_translation = argc > 8 ? std::atoi(argv[8]) : 0;
    // argv[9]：1 时第二帧走双参考复合（LAST 用槽 0 的 TRANSLATION 全局运动、
    // GOLDEN 用槽 1 的 IDENTITY），需要帧头打开 switchable_comp_refs。
    const int compound = argc > 9 ? std::atoi(argv[9]) : 0;
    // argv[10]：复合方式（0 AVG，1 WEDGE，2 DIFFWTD）；argv[11]：wedge_index 0..15；
    // argv[12]：wedge_sign / DIFFWTD 的 mask_type 位。
    const int compound_mode = argc > 10 ? std::atoi(argv[10]) : 0;
    const int wedge_idx = argc > 11 ? std::atoi(argv[11]) : 0;
    const int wedge_sign = argc > 12 ? std::atoi(argv[12]) : 0;
    if (gmv_translation || compound) inter.global_mv_type = 1;
    if (compound) inter.compound_refs = true;
    if (compound_mode >= 1) seq.masked_compound = true;
    // argv[13]：1 时走单参考 inter-intra；argv[14]：ii 模式 0..3；
    // argv[15]：1 时用楔形掩码；argv[16]：ii 楔形索引。
    const int interintra = argc > 13 ? std::atoi(argv[13]) : 0;
    const InterIntra8x8Config ii_cfg{argc > 14 ? std::atoi(argv[14]) : 0,
                                     argc > 15 ? std::atoi(argv[15]) != 0 : false,
                                     argc > 16 ? std::atoi(argv[16]) : 0};
    if (interintra) {
        seq.interintra = true;
        inter.global_mv_type = 1;  // 单参考 MV 由帧头 GMV 平移给出。
    }
    // argv[17]：GMV 类型 2=ROTZOOM、3=AFFINE；argv[18..23]：
    // alpha beta gamma delta trans_x trans_y（前四个为 15 位定点，平移 1/64 像素）。
    const int gm_type = argc > 17 ? std::atoi(argv[17]) : 0;
    const int gm_alpha = argc > 18 ? std::atoi(argv[18]) : 0;
    const int gm_beta = argc > 19 ? std::atoi(argv[19]) : 0;
    int gm_gamma = argc > 20 ? std::atoi(argv[20]) : 0;
    int gm_delta = argc > 21 ? std::atoi(argv[21]) : 0;
    const int gm_tx = argc > 22 ? std::atoi(argv[22]) : 0;
    const int gm_ty = argc > 23 ? std::atoi(argv[23]) : 0;
    if (gm_type == 2) {
        // ROTZOOM 的 gamma/delta 由解码端从矩阵隐式导出：gamma≈-beta，delta≈alpha。
        gm_gamma = -gm_beta;
        gm_delta = gm_alpha;
    }
    if (gm_type >= 2) {
        inter.global_mv_type = gm_type;
        inter.gm_alpha = gm_alpha;
        inter.gm_beta = gm_beta;
        inter.gm_gamma = gm_gamma;
        inter.gm_delta = gm_delta;
        inter.gm_trans_x = gm_tx;
        inter.gm_trans_y = gm_ty;
    }
    // argv[24]：量化矩阵预设；帧头 using_qmatrix + 两个 4 bit 选择子，
    // 残差反量化用同一个 8x8 权重表（与解码端 qm_tbl 逐值一致）。
    const int qm_level = argc > 24 ? std::atoi(argv[24]) : 15;
    if (qm_level < 15) {
        key.using_qmatrix = inter.using_qmatrix = true;
        key.qm_y = inter.qm_y = qm_level;
        key.qm_u = inter.qm_u = qm_level;
    }
    const bool distwtd = compound_mode == 3;
    if (distwtd) {
        seq.order_hint = true;
        seq.jnt_comp = true;
        seq.order_hint_bits = 1;
        key.order_hint = inter.order_hint = true;
        key.order_hint_bits = inter.order_hint_bits = 1;
        inter.frame_offset = 1;
    }
    const Compound8x8Config compound_cfg{
        distwtd ? CompoundMode8x8::DistWtd
                : compound_mode == 1 ? CompoundMode8x8::Wedge
                : compound_mode == 2 ? CompoundMode8x8::DiffWtd
                                     : CompoundMode8x8::Avg,
        wedge_idx, wedge_sign, distwtd};

    Residual8x8 r;
    r.coeff[0] = 12;
    r.coeff[kScan8x8[1]] = 24;
    r.coeff[kScan8x8[2]] = -16;
    r.coeff[kScan8x8[3]] = 9;
    r.coeff[kScan8x8[7]] = 5;
    r.qm_y = qm_level;

    std::string stream = make_temporal_delimiter();
    stream += make_obu(ObuType::SequenceHeader, write_sequence_header_inter(seq));
    stream += make_temporal_delimiter();
    stream += make_obu(ObuType::FrameHeader, write_frame_header_inter(key));
    stream += make_obu(ObuType::TileGroup, write_tile_group_8x8(r));

    // 帧间残差用的系数模式（与参考帧的残差不同，判据才可区分）。
    Residual8x8 ri;
    ri.coeff[0] = -11;
    ri.coeff[kScan8x8[5]] = 17;
    ri.coeff[kScan8x8[9]] = -13;
    ri.coeff[kScan8x8[15]] = 7;
    ri.qm_y = qm_level;
    const Residual8x8 inter_residual = with_residual ? ri : Residual8x8{};

    std::string second = make_temporal_delimiter();
    second += make_obu(ObuType::FrameHeader, write_frame_header_inter(inter));
    second += make_obu(
        ObuType::TileGroup,
        interintra ? write_tile_group_interintra_8x8(mv_x, mv_y, ii_cfg, filter)
                   : compound ? write_tile_group_compound_8x8(compound_cfg)
                              : write_tile_group_inter_8x8(mv_x, mv_y, newmv_probe,
                                                           inter_residual));

    std::string ivf = write_ivf_header(8, 8, 30, 2);
    ivf += write_ivf_frame(stream, 0);
    ivf += write_ivf_frame(second, 1);
    FILE* f = std::fopen(argv[3], "wb");
    if (!f) return 1;
    std::fwrite(ivf.data(), 1, ivf.size(), f);
    std::fclose(f);

    RefFrame8x8 ref;
    ref.y = reconstruct_luma_8x8(r);
    ref.u.fill(128);
    ref.v.fill(128);
    const auto got = gm_type >= 2
                         ? reconstruct_gm_warp_8x8(ref, gm_type, gm_alpha, gm_beta,
                                                   gm_gamma, gm_delta, gm_tx, gm_ty)
                     : interintra ? reconstruct_interintra_8x8(ref, mv_x, mv_y, ii_cfg, filter)
                     : compound ? reconstruct_compound_8x8(ref, mv_x, mv_y, ref, 0, 0,
                                                           compound_cfg, filter)
                         : reconstruct_inter_8x8(ref, newmv_probe >= 0 || gmv_translation ? mv_x : 0,
                                                 newmv_probe >= 0 || gmv_translation ? mv_y : 0,
                                                 inter_residual, filter);
    unsigned char buf[192];
    std::memcpy(buf, ref.y.data(), 64);
    std::memcpy(buf + 64, ref.u.data(), 16);
    std::memcpy(buf + 80, ref.v.data(), 16);
    std::memcpy(buf + 96, got.y.data(), 64);
    std::memcpy(buf + 160, got.u.data(), 16);
    std::memcpy(buf + 176, got.v.data(), 16);
    f = std::fopen(argv[4], "wb");
    if (!f) return 1;
    std::fwrite(buf, 1, sizeof(buf), f);
    std::fclose(f);
    return 0;
}
