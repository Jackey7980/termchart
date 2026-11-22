// av1_inter_probe.cpp —— 阶段五探针：两帧 8x8 IVF，KEY + INTER(GLOBALMV)。
// 用法: av1_inter_probe <mv_x_q3> <mv_y_q3> <out.ivf> <out.yuv>
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

    Residual8x8 r;
    r.coeff[0] = 12;
    r.coeff[kScan8x8[1]] = 24;
    r.coeff[kScan8x8[2]] = -16;
    r.coeff[kScan8x8[3]] = 9;
    r.coeff[kScan8x8[7]] = 5;

    std::string stream = make_temporal_delimiter();
    stream += make_obu(ObuType::SequenceHeader, write_sequence_header_inter(seq));
    stream += make_temporal_delimiter();
    stream += make_obu(ObuType::FrameHeader, write_frame_header_inter(key));
    stream += make_obu(ObuType::TileGroup, write_tile_group_8x8(r));

    std::string second = make_temporal_delimiter();
    second += make_obu(ObuType::FrameHeader, write_frame_header_inter(inter));
    const int joint_override = argc > 5 ? std::atoi(argv[5]) : -1;
    second += make_obu(ObuType::TileGroup,
                       write_tile_group_inter_8x8(mv_x, mv_y, joint_override));

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
    const auto got = reconstruct_inter_8x8(ref, joint_override >= 0 ? mv_x : 0,
                                           joint_override >= 0 ? mv_y : 0);
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
