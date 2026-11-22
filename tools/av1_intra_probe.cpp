// av1_intra_probe.cpp —— 阶段四探针：8x16 帧块 B 使用 filter intra / CFL。
// 用法: av1_intra_probe <y_mode> <angle> <filter> <uv_mode> <cfl_u> <cfl_v>
//                       <cfl_signs> <out.ivf> <out.yuv>
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
    if (argc < 10) {
        std::fprintf(stderr,
                     "usage: av1_intra_probe y angle filter uv cu cv signs ivf yuv\n");
        return 2;
    }
    BlockMode8x8 b;
    b.y_mode = std::atoi(argv[1]);
    b.angle_delta = std::atoi(argv[2]);
    b.filter_intra = std::atoi(argv[3]);
    b.uv_mode = std::atoi(argv[4]);
    b.cfl_u_idx = std::atoi(argv[5]);
    b.cfl_v_idx = std::atoi(argv[6]);
    b.cfl_signs = std::atoi(argv[7]);

    SequenceHeaderConfig seq;
    seq.width = 8;
    seq.height = 16;
    FrameHeaderConfig fh;
    fh.base_q_idx = 100;

    Residual8x8 a;
    a.coeff[0] = 6;
    a.coeff[kScan8x8[1]] = 40;
    a.coeff[kScan8x8[2]] = -25;
    a.coeff[kScan8x8[3]] = 18;

    std::string stream = make_temporal_delimiter();
    stream += make_obu(ObuType::SequenceHeader, write_sequence_header(seq));
    stream += make_temporal_delimiter();
    stream += make_obu(ObuType::FrameHeader, write_frame_header(fh));
    stream += make_obu(ObuType::TileGroup, write_tile_group_8x16(a, b));
    std::string ivf = write_ivf_header(8, 16, 30, 1);
    ivf += write_ivf_frame(stream, 0);

    FILE* f = std::fopen(argv[8], "wb");
    if (!f) return 1;
    std::fwrite(ivf.data(), 1, ivf.size(), f);
    std::fclose(f);

    const auto recon = reconstruct_8x16(a, b);
    unsigned char buf[192];
    std::memcpy(buf, recon.y.data(), 128);
    std::memcpy(buf + 128, recon.u.data(), 32);
    std::memcpy(buf + 160, recon.v.data(), 32);
    f = std::fopen(argv[9], "wb");
    if (!f) return 1;
    std::fwrite(buf, 1, sizeof(buf), f);
    std::fclose(f);
    return 0;
}
