// av1_partition_probe.cpp —— 16x16 帧多层划分 / 4 子块 NEWMV / 逐块 warp 探针。
// 用法: av1_partition_probe <mode> <y_mode> <mv_x_q3> <mv_y_q3> <out.ivf> <out.yuv>
//   mode 0：单帧 KEY，16x16 单块（10 符号 partition NONE + 帧内模式）
//   mode 1：两帧，KEY 同上 + INTER_FRAME 16x16 单块（skip=1 + NEWMV）
//   mode 2：单帧 KEY，16x16 层 PARTITION_SPLIT + 2x2 个 8x8 子块（此时 y_mode
//           参数当作模式组合序号，mv 参数忽略）
//   mode 3：两帧，KEY 为 4 子块纹理帧 + INTER 为 4 个 NEWMV 子块，右下块
//           motion_mode=MM_WARP。参数依次为四个块的 (mv_x,mv_y)，共 8 个数：
//           av1_partition_probe 3 0 m0x m0y m1x m1y m2x m2y m3x m3y ivf yuv
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "codec/av1/bitstream/headers.hpp"
#include "codec/av1/bitstream/ivf_muxer.hpp"
#include "codec/av1/bitstream/obu_header.hpp"
#include "codec/av1/bitstream/tile_group.hpp"
#include "codec/av1/entropy/default_cdf.hpp"

using namespace termchart::image::codec::av1;

namespace {

// 四块的 (y_mode, angle_delta) 组合：覆盖「上/左邻居模式折叠出的 kfym 上下文」
// 与「SMOOTH 族邻居把边界滤波抬到 type 1」两条路径。
struct SplitPattern {
    int y_mode[4];
    int angle_delta[4];
};

const SplitPattern kSplitPatterns[] = {
    {{DC_PRED, HOR_PRED, VERT_PRED, PAETH_PRED}, {0, 0, 0, 0}},
    {{SMOOTH_PRED, SMOOTH_H_PRED, SMOOTH_V_PRED, PAETH_PRED}, {0, 0, 0, 0}},
    {{DC_PRED, VERT_PRED, HOR_PRED, DC_PRED}, {0, 0, 0, 0}},
    {{DC_PRED, HOR_PRED, HOR_PRED, HOR_PRED}, {0, 0, 0, 0}},
    {{DC_PRED, DC_PRED, DC_PRED, DC_PRED}, {0, 0, 0, 0}},
    // 4..8 是八个方向类模式：分别吃 above-right / below-left / 上+左边界，
    // 用来验证子块边沿可用性（右列 above-right 越界、上列 below-left 未重建）。
    {{3, 6, 4, 7}, {0, 0, 0, 0}},
};
constexpr int kSplitPatternCount =
    static_cast<int>(sizeof(kSplitPatterns) / sizeof(kSplitPatterns[0]));

Blocks16x16 make_split_blocks(int pattern) {
    Blocks16x16 blocks;
    const SplitPattern& p =
        kSplitPatterns[static_cast<std::size_t>(pattern % kSplitPatternCount)];
    for (int k = 0; k < 4; ++k) {
        BlockMode8x8& m = blocks.modes[static_cast<std::size_t>(k)];
        m.y_mode = p.y_mode[k];
        m.angle_delta = p.angle_delta[k];
        m.uv_mode = DC_PRED;
        // 首块（无邻居）带 DC+AC 残差制造纹理；其余块 skip=1，只做帧内预测，
        // 这样不会踩到需要邻居 res_ctx 的 txb_skip 上下文表。
        blocks.skip[static_cast<std::size_t>(k)] = k == 0 ? 0 : 1;
    }
    blocks.residuals[0].coeff[0] = 48;
    blocks.residuals[0].coeff[8] = -24;
    return blocks;
}

void append_frame(std::string& dst, const Frame16x16& f) {
    dst.append(reinterpret_cast<const char*>(f.y.data()), f.y.size());
    dst.append(reinterpret_cast<const char*>(f.u.data()), f.u.size());
    dst.append(reinterpret_cast<const char*>(f.v.data()), f.v.size());
}

bool write_file(const char* path, const std::string& data) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const std::size_t n = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return n == data.size();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 7) {
        std::fprintf(stderr,
                     "usage: av1_partition_probe mode ymode mvx mvy ivf yuv\n");
        return 2;
    }
    const int mode = std::atoi(argv[1]);
    if (mode == 3 && argc < 13) {
        std::fprintf(stderr, "mode 3 needs 8 MV components + ivf + yuv\n");
        return 2;
    }
    BlockMode8x8 block;
    block.y_mode = std::atoi(argv[2]);
    const int mv_x = std::atoi(argv[3]);
    const int mv_y = std::atoi(argv[4]);
    WarpSplit16x16Mvs warp_mvs;
    if (mode == 3) {
        for (int k = 0; k < 4; ++k) {
            warp_mvs.mv_x[k] = std::atoi(argv[3 + 2 * k]);
            warp_mvs.mv_y[k] = std::atoi(argv[4 + 2 * k]);
        }
    }
    const bool split_inter = mode == 3;
    const char* ivf_path = split_inter ? argv[11] : argv[5];
    const char* yuv_path = split_inter ? argv[12] : argv[6];
    const Blocks16x16 split = make_split_blocks(mode == 2 ? block.y_mode : 0);

    SequenceHeaderConfig seq;
    seq.width = 16;
    seq.height = 16;
    seq.inter_frames = mode == 1 || split_inter;

    FrameHeaderConfig key;
    key.frame_type = 0;
    key.base_q_idx = 100;

    FrameHeaderConfig inter;
    inter.frame_type = 1;
    inter.base_q_idx = 100;
    if (split_inter) {
        inter.switchable_motion_mode = true;
        inter.error_resilient = false;
        inter.warp_motion = true;
    }

    std::string first = make_temporal_delimiter();
    first += make_obu(ObuType::SequenceHeader, write_sequence_header_inter(seq));
    first += make_temporal_delimiter();
    first += make_obu(ObuType::FrameHeader, write_frame_header_inter(key));
    first += make_obu(ObuType::TileGroup,
                      mode == 2 || split_inter
                          ? write_tile_group_intra_16x16_split(split)
                          : write_tile_group_intra_16x16(block));

    std::string second;
    if (mode == 1 || split_inter) {
        second += make_temporal_delimiter();
        second += make_obu(ObuType::FrameHeader, write_frame_header_inter(inter));
        second += make_obu(ObuType::TileGroup,
                           split_inter
                               ? write_tile_group_warp_split_16x16(warp_mvs)
                               : write_tile_group_inter_16x16(mv_x, mv_y));
    }

    const int frames = mode == 1 || split_inter ? 2 : 1;
    std::string ivf = write_ivf_header(16, 16, 30, frames);
    ivf += write_ivf_frame(first, 0);
    if (frames == 2) ivf += write_ivf_frame(second, 1);
    if (!write_file(ivf_path, ivf)) return 1;

    const Frame16x16 key_recon = mode == 2 || split_inter
                                     ? reconstruct_intra_16x16_split(split)
                                     : reconstruct_intra_16x16(block);
    std::string yuv;
    append_frame(yuv, key_recon);
    if (mode == 1) {
        append_frame(yuv, reconstruct_inter_16x16(key_recon, mv_x, mv_y));
    } else if (split_inter) {
        append_frame(yuv, reconstruct_warp_split_16x16(key_recon, warp_mvs));
    }
    if (!write_file(yuv_path, yuv)) return 1;
    return 0;
}
