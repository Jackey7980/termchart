// av1_deblock_probe.cpp —— 阶段七探针：8x16 KEY 帧 + 环路去块滤波。
// 用法: av1_deblock_probe <level_y> <sharpness> <out.ivf> <out.yuv>
// 期望 YUV 由本库的去块滤波器生成，再用 ffmpeg/libdav1d 解码码流逐像素对拍。
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
#include "codec/av1/entropy/default_cdf.hpp"
#include "codec/av1/postproc/deblock.hpp"

using namespace termchart::image::codec::av1;

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr, "usage: av1_deblock_probe level sharpness ivf yuv\n");
        return 2;
    }
    const int level = std::atoi(argv[1]) & 63;
    const int sharpness = std::atoi(argv[2]) & 7;

    SequenceHeaderConfig seq;
    seq.width = 8;
    seq.height = 16;
    FrameHeaderConfig key;
    key.frame_type = 0;
    key.base_q_idx = 100;
    key.loop_filter_level_y = level;
    key.loop_filter_level_uv = 0;
    key.loop_filter_sharpness = sharpness;

    Residual8x8 residual;
    residual.coeff[0] = 9;
    residual.coeff[kScan8x8[1]] = 21;
    residual.coeff[kScan8x8[2]] = -14;
    residual.coeff[kScan8x8[6]] = 11;
    BlockMode8x8 mode_b;
    mode_b.y_mode = VERT_PRED;

    std::string stream = make_temporal_delimiter();
    stream += make_obu(ObuType::SequenceHeader, write_sequence_header_inter(seq));
    stream += make_temporal_delimiter();
    stream += make_obu(ObuType::FrameHeader, write_frame_header_inter(key));
    stream += make_obu(ObuType::TileGroup,
                       write_tile_group_8x16(residual, mode_b));

    const std::string ivf = write_ivf_header(8, 16, 30, 1) +
                            write_ivf_frame(stream, 0);
    FILE* f = std::fopen(argv[3], "wb");
    if (!f) return 1;
    std::fwrite(ivf.data(), 1, ivf.size(), f);
    std::fclose(f);

    auto frame = reconstruct_8x16(residual, mode_b);
    av1_deblock_luma_grid(frame.y.data(), 8, 8, 16,
                          Av1DeblockParams{level, sharpness});
    unsigned char buf[192];
    std::memcpy(buf, frame.y.data(), 128);
    std::memcpy(buf + 128, frame.u.data(), 32);
    std::memcpy(buf + 160, frame.v.data(), 32);
    f = std::fopen(argv[4], "wb");
    if (!f) return 1;
    std::fwrite(buf, 1, sizeof(buf), f);
    std::fclose(f);
    return 0;
}
