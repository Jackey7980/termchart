// test_av1_obu.cpp —— OBU 头 / LEB128 / IVF 容器 / 8x8 块语法的字节级回环
#include <doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include "src/image/codec/av1/bitstream/ivf_muxer.hpp"
#include "src/image/codec/av1/bitstream/obu_header.hpp"
#include "src/image/codec/av1/bitstream/partition_tree.hpp"
#include "src/image/codec/av1/bitstream/tile_group.hpp"
#include "src/image/codec/av1/entropy/cdf_manager.hpp"
#include "src/image/codec/av1/entropy/coef_cdf_data.hpp"
#include "src/image/codec/av1/entropy/default_cdf.hpp"
#include "src/image/codec/av1/entropy/range_coder.hpp"
#include "src/image/codec/av1/predict/inter_predict.hpp"
#include "src/image/codec/av1/predict/intra_predict.hpp"

namespace av1 = termchart::image::codec::av1;

namespace {

struct TileSymbols {
    int skip;
    unsigned y;
    int angle_delta;
    int filter_intra;
    unsigned uv;
};

// 8x8 层的划分符号：帧左上角上下文为 0，PARTITION_NONE 之后仍是 0。
// 与 tile_group.cpp::write_partition_none 成对。
unsigned read_partition_none(av1::RangeDecoder& dec) {
    return av1::read_symbol(dec, av1::kDefaultCdf.m.partition[4][0], 4);
}

// 单块帧内模式语法，顺序与 tile_group.cpp::write_block_mode 一致：
// skip → y_mode → angle_delta（方向模式才有）→ uv_mode → use_filter_intra（仅 DC）。
TileSymbols read_block_mode(av1::RangeDecoder& dec) {
    const auto& m = av1::kDefaultCdf;
    TileSymbols s{};
    s.skip = dec.decode_bool(m.m.skip[0][0]);
    s.y = av1::read_symbol(dec, m.kfym[0][0], N_INTRA_PRED_MODES);
    if (s.y >= VERT_PRED && s.y <= VERT_LEFT_PRED)
        s.angle_delta =
            static_cast<int>(av1::read_symbol(dec, m.m.angle_delta[s.y - 1], 7)) -
            3;  // 语法范围 -3..3
    s.uv = av1::read_symbol(dec, m.m.uv_mode[1][s.y], N_UV_INTRA_PRED_MODES);
    if (s.y == DC_PRED) s.filter_intra = dec.decode_bool(m.m.use_filter_intra[3][0]);
    return s;
}

TileSymbols decode_flat_tile(const std::uint8_t* data, std::size_t size) {
    av1::RangeDecoder dec;
    dec.init(data, size);
    REQUIRE(read_partition_none(dec) == PARTITION_NONE);
    return read_block_mode(dec);
}

struct ResidualDecoder {
    av1::RangeDecoder dec;
    std::array<unsigned char, 12 * 12> levels{};

    static std::size_t at(int x, int y) {
        return static_cast<std::size_t>((y + 2) * 12 + (x + 2));
    }
    unsigned char level(int x, int y) const { return levels[at(x, y)]; }
    void set_level(int rc, int magnitude) {
        levels[at(rc >> 3, rc & 7)] = static_cast<unsigned char>(magnitude);
    }

    unsigned low_ctx(int rc) const {
        // 与编码端一致：DC（coeff_idx 0）的 base 上下文恒为 0
        // （libaom get_nz_map_ctx_from_stats 直接 return 0）。
        if (rc == 0) return 0u;
        const int x = rc >> 3, y = rc & 7;
        const int mag = std::min<int>(level(x, y + 1), 3) +
                        std::min<int>(level(x + 1, y), 3) +
                        std::min<int>(level(x + 1, y + 1), 3) +
                        std::min<int>(level(x, y + 2), 3) +
                        std::min<int>(level(x + 2, y), 3);
        return av1::kLoCtxOffsetSquare[std::min(y, 4)][std::min(x, 4)] +
               static_cast<unsigned>(std::min((mag + 1) >> 1, 4));
    }

    unsigned high_ctx(int rc) const {
        const int x = rc >> 3, y = rc & 7;
        const int mag = level(x + 1, y) + level(x, y + 1) + level(x + 1, y + 1);
        const unsigned ctx = static_cast<unsigned>(std::min((mag + 1) >> 1, 6));
        if (rc == 0) return ctx;
        return ctx + (((x | y) < 2) ? 7u : 14u);
    }

    static unsigned eob_high_ctx(int rc) {
        if (rc == 0) return 0;
        const int x = rc >> 3, y = rc & 7;
        return ((x | y) < 2) ? 7u : 14u;
    }

    int high_token(unsigned ctx) {
        int extra = 0;
        for (int shift = 3; shift <= 12; shift += 3) {
            const int part = static_cast<int>(
                av1::read_symbol(dec, av1::kBrTokLuma[ctx], 4));
            extra += part;
            if (part < 3) break;
        }
        return 3 + extra;
    }

    int token(unsigned ctx, int rc) {
        const int t = static_cast<int>(
            av1::read_symbol(dec, av1::kBaseTokLuma[ctx], 4));
        return t < 3 ? t : high_token(high_ctx(rc));
    }

    int eob_token(unsigned ctx, int rc) {
        const int t = static_cast<int>(
            av1::read_symbol(dec, av1::kEobBaseTokLuma[ctx], 3));
        return t < 2 ? t + 1 : high_token(eob_high_ctx(rc));
    }
};

std::array<std::int16_t, 64> decode_residual_tile(const std::string& payload) {
    // 单 tile 帧不写 tile_start_and_end_present_flag / tile_size，
    // OBU 载荷就是裸 tile data（与 libaom / dav1d 一致）。
    REQUIRE(!payload.empty());
    ResidualDecoder s;
    s.dec.init(reinterpret_cast<const std::uint8_t*>(payload.data()),
               static_cast<std::uint32_t>(payload.size()));
    REQUIRE(read_partition_none(s.dec) == PARTITION_NONE);
    (void)read_block_mode(s.dec);
    std::array<std::int16_t, 64> out{};
    if (s.dec.decode_bool(av1::kTxBypassLuma8x8[0])) return out;

    // reduced_tx_set=1 下 intra 8x8 属于 EXT_TX_SET_DTT4_IDTX，共 5 个符号。
    av1::read_symbol(s.dec, av1::kTxTypeIntra8x8Smooth, 5);
    const int eob_pt = static_cast<int>(
                           av1::read_symbol(s.dec, av1::kEobBin64Luma, 7)) +
                       1;
    int eob = 0;
    if (eob_pt == 1) {
        eob = 1;
    } else if (eob_pt == 2) {
        eob = 2;
    } else {
        const int bits = eob_pt - 2;
        const int hi = s.dec.decode_bool(av1::kEobHiBitLuma[eob_pt - 3][0]);
        int low = 0;
        for (int i = 1; i < bits; ++i)
            low = (low << 1) | s.dec.decode_bool(16384);
        eob = (1 << bits) + 1 + ((hi << (bits - 1)) | low);
    }

    const auto scan_at = [](int i) { return av1::kScan8x8[static_cast<std::size_t>(i)]; };
    const int eob_rc = scan_at(eob - 1);
    const unsigned eob_ctx = eob <= 1 ? 0u : (eob <= 9 ? 1u : (eob <= 17 ? 2u : 3u));
    const int eob_mag = s.eob_token(eob_ctx, eob_rc);
    out[static_cast<std::size_t>(eob_rc)] = static_cast<std::int16_t>(eob_mag);
    s.set_level(eob_rc, eob_mag);

    for (int i = eob - 2; i >= 0; --i) {
        const int rc = scan_at(i);
        const int mag = s.token(s.low_ctx(rc), rc);
        out[static_cast<std::size_t>(rc)] = static_cast<std::int16_t>(mag);
        s.set_level(rc, mag);
    }

    int first_ac = 0;
    if (out[0] != 0) {
        s.dec.decode_bool(av1::kDcSignLuma[0]);
        first_ac = 1;
    }
    for (int i = first_ac; i < eob; ++i) {
        const int rc = scan_at(i);
        if (out[static_cast<std::size_t>(rc)] != 0) s.dec.decode_bool(16384);
    }
    return out;
}

}  // namespace

TEST_CASE("av1: LEB128 编码与边界") {
    CHECK(av1::write_leb128(0) == std::string("\0", 1));
    CHECK(av1::write_leb128(1) == std::string("\1", 1));
    CHECK(av1::write_leb128(127) == std::string("\x7f", 1));
    CHECK(av1::write_leb128(128) == std::string("\x80\x01", 2));
    CHECK(av1::write_leb128(16383) == std::string("\xff\x7f", 2));
    CHECK(av1::write_leb128(16384) == std::string("\x80\x80\x01", 3));
}

TEST_CASE("av1: OBU 头字节与回读") {
    const av1::ObuType types[] = {
        av1::ObuType::SequenceHeader,   av1::ObuType::TemporalDelimiter,
        av1::ObuType::FrameHeader,      av1::ObuType::TileGroup,
    };
    const std::uint8_t headers[] = {0x0a, 0x12, 0x1a, 0x22};
    for (std::size_t i = 0; i < 4; ++i) {
        const std::string payload = std::string("abcd\0xyz", 8);
        const std::string obu = av1::make_obu(types[i], payload);
        CHECK(static_cast<unsigned char>(obu[0]) == headers[i]);
        av1::ObuType got = av1::ObuType::Padding;
        std::string_view body;
        std::size_t used = 0;
        REQUIRE(av1::read_obu(obu, 0, got, body, used));
        CHECK(got == types[i]);
        CHECK(body == std::string_view(payload));
        CHECK(used == obu.size());
    }
    CHECK(av1::make_temporal_delimiter() == std::string("\x12\0", 2));
}

TEST_CASE("av1: IVF 头字段与帧长") {
    const std::string h = av1::write_ivf_header(8, 8, 30, 1);
    REQUIRE(h.size() == 32);
    CHECK(h.substr(0, 4) == "DKIF");
    CHECK(h.substr(8, 4) == "AV01");
    CHECK(static_cast<unsigned char>(h[4]) == 0);
    CHECK(static_cast<unsigned char>(h[6]) == 32);
    CHECK(static_cast<unsigned char>(h[12]) == 8);
    CHECK(static_cast<unsigned char>(h[14]) == 8);

    const std::string frame = av1::write_ivf_frame(std::string(7, 'z'), 3);
    REQUIRE(frame.size() == 19);
    CHECK(static_cast<unsigned char>(frame[0]) == 7);
    CHECK(static_cast<unsigned char>(frame[4]) == 3);
    CHECK(frame.substr(12) == std::string(7, 'z'));
}

TEST_CASE("av1: 最小 tile group 字节序列") {
    const std::string tg = av1::write_tile_group_8x8();
    REQUIRE(tg.size() == 2);
    // partition NONE + skip 0 + y_mode SMOOTH + uv_mode DC + 亮度 tx_skip
    // + 两条色度 all_zero 的区间编码结果，已由 libaom aomdec / dav1d 解码确认
    //（8x8 帧逐像素一致）。
    CHECK(static_cast<unsigned char>(tg[0]) == 0x70);
    CHECK(static_cast<unsigned char>(tg[1]) == 0x78);

    const TileSymbols sym = decode_flat_tile(
        reinterpret_cast<const std::uint8_t*>(tg.data()),
        static_cast<std::uint32_t>(tg.size()));
    CHECK(sym.skip == 0);
    CHECK(sym.y == static_cast<unsigned>(SMOOTH_PRED));
    CHECK(sym.angle_delta == 0);
    CHECK(sym.filter_intra == 0);
    CHECK(sym.uv == static_cast<unsigned>(DC_PRED));

    // 无邻居时边界填 above=127 / left=129，SMOOTH 预测在块内产生 127/128/129
    // 三种值；下面这 64 个灰度与 aomdec 解码结果逐像素一致。
    const auto y = av1::reconstruct_luma_8x8(av1::Residual8x8{});
    constexpr std::array<std::uint8_t, 64> kExpected = {
        128, 128, 128, 127, 127, 127, 127, 127, 128, 128, 128, 128, 128, 127,
        127, 127, 128, 128, 128, 128, 128, 128, 128, 128, 129, 128, 128, 128,
        128, 128, 128, 128, 129, 128, 128, 128, 128, 128, 128, 128, 129, 129,
        128, 128, 128, 128, 128, 128, 129, 129, 128, 128, 128, 128, 128, 128,
        129, 129, 128, 128, 128, 128, 128, 128};
    CHECK(y == kExpected);
}

TEST_CASE("av1: 8x8 DCT8 残差的确定性重建") {
    av1::Residual8x8 r;
    r.coeff[0] = 6;
    r.coeff[av1::kScan8x8[1]] = 3;
    r.coeff[av1::kScan8x8[2]] = -2;

    const std::string tg = av1::write_tile_group_8x8(r);
    CHECK(tg.size() > 4);
    const auto y = av1::reconstruct_luma_8x8(r);
    bool non_flat = false;
    for (auto v : y) non_flat = non_flat || (v != 128);
    CHECK(non_flat);
    CHECK(y == av1::reconstruct_luma_8x8(r));
}

TEST_CASE("av1: 残差符号熵编解码往返") {
    av1::Residual8x8 r;
    r.coeff[0] = -8;
    r.coeff[av1::kScan8x8[1]] = 12;
    r.coeff[av1::kScan8x8[4]] = -3;
    r.coeff[av1::kScan8x8[9]] = 6;

    const auto decoded = decode_residual_tile(av1::write_tile_group_8x8(r));
    for (std::size_t i = 0; i < r.coeff.size(); ++i) {
        CHECK(decoded[i] == static_cast<std::int16_t>(std::abs(r.coeff[i])));
    }
}

TEST_CASE("av1: 十叉树划分与邻居上下文往返") {
    using PT = av1::PartitionType;
    using Node = av1::PartitionNode;
    av1::PartitionContext enc_ctx;
    enc_ctx.frame_mi_rows = 16;
    enc_ctx.frame_mi_cols = 16;
    const auto choose = [](int row, int col, int w, int, bool, bool) -> PT {
        if (w == 7) return PT::Split;
        if (w == 6) {
            if (row == 0 && col == 0) return PT::Horizontal;
            if (row == 0 && col == 8) return PT::Vertical;
            if (row == 8 && col == 0) return PT::TopSplit;
            return PT::BottomSplit;
        }
        if (w == 5) {
            if (row == 0) return PT::LeftSplit;
            if (row == 8) return PT::RightSplit;
            if (col == 0) return PT::Horizontal4;
            return PT::Vertical4;
        }
        if (w == 4) return PT::Split;
        return PT::None;
    };

    av1::RangeEncoder enc;
    const Node written = av1::write_partition_tree(
        enc, av1::kDefaultCdf.m.partition, enc_ctx, 0, 0, 7, choose);
    const std::string bytes = enc.finish();

    av1::PartitionContext dec_ctx;
    dec_ctx.frame_mi_rows = 16;
    dec_ctx.frame_mi_cols = 16;
    av1::RangeDecoder dec;
    dec.init(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    const Node read = av1::read_partition_tree(
        dec, av1::kDefaultCdf.m.partition, dec_ctx, 0, 0, 7);

    const auto same = [](const auto& self, const Node& a, const Node& b) -> bool {
        if (a.row != b.row || a.col != b.col || a.w_log2 != b.w_log2 ||
            a.h_log2 != b.h_log2 || a.type != b.type ||
            a.children.size() != b.children.size())
            return false;
        for (std::size_t i = 0; i < a.children.size(); ++i)
            if (!self(self, a.children[i], b.children[i])) return false;
        return true;
    };
    CHECK(same(same, written, read));
    CHECK(enc_ctx.above == dec_ctx.above);
    CHECK(enc_ctx.left == dec_ctx.left);
}

TEST_CASE("av1: 边界块强制划分使用二符号合并 CDF") {
    using PT = av1::PartitionType;
    av1::PartitionContext enc_ctx;
    enc_ctx.frame_mi_rows = 8;
    enc_ctx.frame_mi_cols = 16;
    const auto choose = [](int, int, int, int, bool, bool) { return PT::Horizontal; };

    av1::RangeEncoder enc;
    const auto written = av1::write_partition_tree(
        enc, av1::kDefaultCdf.m.partition, enc_ctx, 0, 0, 6, choose);
    CHECK(written.type == PT::Horizontal);
    const std::string bytes = enc.finish();

    av1::PartitionContext dec_ctx;
    dec_ctx.frame_mi_rows = 8;
    dec_ctx.frame_mi_cols = 16;
    av1::RangeDecoder dec;
    dec.init(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    const auto read = av1::read_partition_tree(
        dec, av1::kDefaultCdf.m.partition, dec_ctx, 0, 0, 6);
    CHECK(read.type == written.type);
    CHECK(enc_ctx.above == dec_ctx.above);
    CHECK(enc_ctx.left == dec_ctx.left);

    av1::PartitionContext forced_enc, forced_dec;
    forced_enc.frame_mi_rows = forced_dec.frame_mi_rows = 8;
    forced_enc.frame_mi_cols = forced_dec.frame_mi_cols = 8;
    av1::RangeEncoder forced_writer;
    const auto forced = av1::write_partition_tree(
        forced_writer, av1::kDefaultCdf.m.partition, forced_enc, 0, 0, 6, choose);
    CHECK(forced.type == PT::Split);
    const std::string forced_bytes = forced_writer.finish();
    av1::RangeDecoder forced_reader;
    forced_reader.init(reinterpret_cast<const std::uint8_t*>(forced_bytes.data()),
                       forced_bytes.size());
    const auto forced_read = av1::read_partition_tree(
        forced_reader, av1::kDefaultCdf.m.partition, forced_dec, 0, 0, 6);
    CHECK(forced_read.type == forced.type);
}

TEST_CASE("av1: filter intra 五档 8x8 预测") {
    unsigned char above[16], left[16], dst[64];
    for (int i = 0; i < 16; ++i) {
        above[i] = static_cast<unsigned char>(100 + i * 7);
        left[i] = static_cast<unsigned char>(130 - i * 5);
    }
    av1::IntraSamples s;
    s.above = above;
    s.left = left;
    s.num_above = 8;
    s.num_left = 8;
    // 参考值来自规范 7.11.2.3 的 8 抽头表，已由 ffmpeg/libdav1d 逐像素对拍。
    constexpr int kExpected[5][8] = {
        {112, 113, 117, 119, 124, 130, 135, 142},
        {101, 108, 115, 121, 128, 135, 142, 149},
        {116, 120, 123, 127, 131, 134, 138, 141},
        {115, 113, 114, 118, 123, 128, 134, 141},
        {105, 111, 117, 123, 129, 135, 141, 147},
    };
    for (int mode = 0; mode < 5; ++mode) {
        av1::predict_filter_intra(s, 8, 8, mode, dst, 8);
        for (int x = 0; x < 8; ++x) CHECK(dst[x] == kExpected[mode][x]);
    }
}

TEST_CASE("av1: CFL 色度端对端重建") {
    av1::Residual8x8 a;
    a.coeff[0] = 6;
    a.coeff[av1::kScan8x8[1]] = 40;
    a.coeff[av1::kScan8x8[2]] = -25;
    a.coeff[av1::kScan8x8[3]] = 18;
    av1::BlockMode8x8 b;
    b.y_mode = SMOOTH_PRED;
    b.uv_mode = CFL_PRED;
    b.cfl_signs = 0;  // U 为零，V 为负
    b.cfl_v_idx = 3;
    const auto recon = av1::reconstruct_8x16(a, b);
    // 已由 ffmpeg/libdav1d 对拍的 CFL 参考像素（块 B 的 V 平面首行）。
    CHECK(recon.y[64] == 255);
    CHECK(recon.u[16] == 128);
    CHECK(recon.v[16] == 111);
    CHECK(recon.v[17] == 122);
    CHECK(recon.v[18] == 141);
    CHECK(recon.v[19] == 156);
    const std::string tile = av1::write_tile_group_8x16(a, b);
    CHECK_FALSE(tile.empty());
}

TEST_CASE("av1: 帧间 8 抽头插值与整像素拷贝") {
    std::uint8_t ref[64], dst[64];
    for (int i = 0; i < 64; ++i) ref[i] = static_cast<std::uint8_t>(i * 3 + 7);
    av1::inter_predict_block(ref, 8, 8, 8, dst, 8, 8, 8, -8, 0);
    for (int y = 0; y < 8; ++y)
        for (int x = 1; x < 8; ++x)
            CHECK(dst[y * 8 + x] == ref[y * 8 + x - 1]);
    // 相位以 1/16 样点为单位：8 = 半样点。
    CHECK(av1::interp_filter_taps(av1::InterpFilter::Regular, 8)[3] == 76);
    CHECK(av1::interp_filter_taps(av1::InterpFilter::Bilinear, 8)[3] == 64);
    std::uint8_t a[4] = {10, 20, 30, 40}, b[4] = {20, 30, 40, 50}, out[4] = {};
    av1::inter_compound_average(a, b, out, 4);
    CHECK(out[0] == 15);
    av1::inter_compound_distance(a, b, -6, out, 4);
    CHECK(out[0] == 11);
    std::uint8_t wt[4][4] = {};
    for (auto& row : wt) for (auto& v : row) v = 16;
    std::uint8_t dst4[16], nb[16];
    for (int i = 0; i < 16; ++i) { dst4[i] = 100; nb[i] = 200; }
    av1::inter_obmc_blend(dst4, 4, nb, 4, wt, 4, 4);
    CHECK(dst4[0] == 150);
    const std::int32_t identity[6] = {0, 0, 65536, 0, 0, 65536};
    av1::warp_affine_predict(ref, 8, 8, 8, dst, 8, 0, 0, 8, 8, identity);
    for (int i = 0; i < 64; ++i) CHECK(dst[i] == ref[i]);
}

TEST_CASE("av1: 帧间亚像素取整模型（16 倍中间精度）") {
    // 平坦参考：任何相位都应原样还原（插值滤波器增益为 1）。
    std::uint8_t flat[64], dst[64];
    for (auto& p : flat) p = 128;
    for (int fy = 0; fy < 8; ++fy)
        for (int fx = 0; fx < 8; ++fx) {
            av1::inter_predict_block(flat, 8, 8, 8, dst, 8, 8, 8, fx, fy);
            for (int i = 0; i < 64; ++i) CHECK(dst[i] == 128);
        }
    // 只有水平方向亚像素时，中间量保留 4 位小数：
    // 结果必须等于 (Σ+4)>>3 之后再 (+8)>>4 的两级取整（与参考实现一致）。
    std::uint8_t ref[64];
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            ref[y * 8 + x] = static_cast<std::uint8_t>(x * 20 + y * 7);
    for (int f = 1; f < 8; ++f) {
        av1::inter_predict_block(ref, 8, 8, 8, dst, 8, 8, 8, f, 0);
        const std::int16_t* taps =
            av1::interp_filter_taps(av1::InterpFilter::Regular, f * 2);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                int sum = 0;
                for (int t = 0; t < 8; ++t) {
                    int sx = x - 3 + t;
                    sx = sx < 0 ? 0 : (sx > 7 ? 7 : sx);
                    sum += ref[y * 8 + sx] * taps[t];
                }
                const int mid = (sum + 4) >> 3;  // 4 位小数的中间量
                CHECK(dst[y * 8 + x] == ((mid + 8) >> 4));
            }
    }
    // 4:2:0 色度：整数偏移取亮度 MV >> 4，1/16 相位取低 4 位。
    // mv=19 ⇒ 偏移 1 个色度样点 + 相位 3（亮度 19/8 像素 = 色度 9.5/8 样点）。
    std::uint8_t cu[16], cv[16], out_u[16], out_v[16];
    for (int i = 0; i < 16; ++i) {
        cu[i] = static_cast<std::uint8_t>(i * 5 + 10);
        cv[i] = static_cast<std::uint8_t>(255 - i * 5);
    }
    av1::inter_predict_chroma(cu, cv, 4, 4, 4, out_u, out_v, 4, 4, 4, 19, 0);
    const std::int16_t* taps3 =
        av1::interp_filter_taps(av1::InterpFilter::Regular, 3);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            for (const std::uint8_t* plane : {cu, cv}) {
                int sum = 0;
                for (int t = 0; t < 8; ++t) {
                    int sx = x + 1 - 3 + t;
                    sx = sx < 0 ? 0 : (sx > 3 ? 3 : sx);
                    sum += plane[y * 4 + sx] * taps3[t];
                }
                const int expect = ((((sum + 4) >> 3)) + 8) >> 4;
                const std::uint8_t got = (plane == cu ? out_u : out_v)[y * 4 + x];
                CHECK(got == expect);
            }
        }
}

TEST_CASE("av1: 两帧 INTER 参考帧重建（零 MV）") {
    av1::RefFrame8x8 ref;
    for (std::size_t i = 0; i < ref.y.size(); ++i)
        ref.y[i] = static_cast<std::uint8_t>(40 + i);
    const auto got = av1::reconstruct_inter_8x8(ref, 0, 0);
    CHECK(got.y == ref.y);
    CHECK_FALSE(av1::write_tile_group_inter_8x8().empty());
}
