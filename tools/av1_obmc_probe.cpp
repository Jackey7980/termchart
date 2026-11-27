// av1_obmc_probe.cpp —— 阶段五 OBMC 探针：8x16 两帧 IVF，KEY 给出非平坦参考，
// INTER 帧上下两个 8x8 块各一个 NEWMV，下块 motion_mode = MM_OBMC。
// 用法: av1_obmc_probe mv_ax mv_ay mv_bx mv_by <out.ivf> <out.yuv>
// 输出的 yuv 是 [KEY 帧重建 192 字节][OBMC 帧重建 192 字节]，与解码输出逐字节对齐。
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
    if (argc < 6) {
        std::fprintf(stderr,
                     "usage: av1_obmc_probe mv_ax mv_ay mv_bx mv_by ivf yuv\n");
        return 2;
    }
    const int mv_a_x = std::atoi(argv[1]);
    const int mv_a_y = std::atoi(argv[2]);
    const int mv_b_x = std::atoi(argv[3]);
    const int mv_b_y = std::atoi(argv[4]);
    const bool obmc_enabled = argc > 7 ? std::atoi(argv[7]) != 0 : true;
    const bool switchable = argc > 8 ? std::atoi(argv[8]) != 0 : true;
    const bool warp = argc > 9 ? std::atoi(argv[9]) != 0 : false;

    SequenceHeaderConfig seq;
    seq.width = 8;
    seq.height = 16;
    seq.inter_frames = true;

    FrameHeaderConfig key;
    key.frame_type = 0;
    key.base_q_idx = 100;

    FrameHeaderConfig inter;
    inter.frame_type = 1;
    inter.base_q_idx = 100;
    inter.switchable_motion_mode = switchable;
    inter.error_resilient = !warp;
    inter.warp_motion = warp;

    // 参考帧要有垂直方向的结构，否则 B 区两种 MV 的预测没有差别，OBMC 掩码就测不出来。
    Residual8x8 r;
    r.coeff[0] = 18;
    r.coeff[kScan8x8[1]] = 31;
    r.coeff[kScan8x8[2]] = -22;
    r.coeff[kScan8x8[3]] = 13;
    r.coeff[kScan8x8[7]] = 7;

    BlockMode8x8 mode_b;  // 默认 SMOOTH_PRED，给出非平坦的下半帧纹理

    std::string stream = make_temporal_delimiter();
    stream += make_obu(ObuType::SequenceHeader, write_sequence_header_inter(seq));
    stream += make_temporal_delimiter();
    stream += make_obu(ObuType::FrameHeader, write_frame_header_inter(key));
    stream += make_obu(ObuType::TileGroup, write_tile_group_8x16(r, mode_b));

    std::string second = make_temporal_delimiter();
    second += make_obu(ObuType::FrameHeader, write_frame_header_inter(inter));
    second += make_obu(ObuType::TileGroup,
                       warp ? write_tile_group_warp_8x16(mv_a_x, mv_a_y)
                            : write_tile_group_obmc_8x16(mv_a_x, mv_a_y, mv_b_x,
                                                         mv_b_y, obmc_enabled));

    std::string ivf = write_ivf_header(8, 16, 30, 2);
    ivf += write_ivf_frame(stream, 0);
    ivf += write_ivf_frame(second, 1);
    FILE* f = std::fopen(argv[5], "wb");
    if (!f) return 1;
    std::fwrite(ivf.data(), 1, ivf.size(), f);
    std::fclose(f);

    // 参考帧 = KEY 帧自重建，与解码端第一帧一致。
    const Reconstructed8x16 ref = reconstruct_8x16(r, mode_b);
    const Reconstructed8x16 got =
        warp ? reconstruct_warp_identity_8x16(ref, mv_a_x, mv_a_y)
             : reconstruct_obmc_8x16(ref, mv_a_x, mv_a_y, mv_b_x, mv_b_y);

    unsigned char buf[384];
    std::memcpy(buf, ref.y.data(), 128);
    std::memcpy(buf + 128, ref.u.data(), 32);
    std::memcpy(buf + 160, ref.v.data(), 32);
    std::memcpy(buf + 192, got.y.data(), 128);
    std::memcpy(buf + 320, got.u.data(), 32);
    std::memcpy(buf + 352, got.v.data(), 32);
    f = std::fopen(argv[6], "wb");
    if (!f) return 1;
    std::fwrite(buf, 1, sizeof(buf), f);
    std::fclose(f);
    return 0;
}
