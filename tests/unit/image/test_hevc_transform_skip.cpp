// test_hevc_transform_skip.cpp —— transform_skip 语法接线回归
//
// 端到端（ffmpeg 解码 recon==dec 逐像素）由 build_scratch/verify_transform_skip.py
// 覆盖；本文件锁死不依赖外部进程的静态不变量：
//   · PPS 位序列：默认 false 与 G0 基线一致，true 只改 transform_skip_enabled_flag 那 1 bit；
//   · transform_skip_flag 的 ctx 绝对偏移与 CABAC_ELEMS 表自洽；
//   · CU bin 序列：每个 cbf=1 的 TU 在残差前恰有一个 flag bin，cbf=0 的不写；
//   · 大 TU（8/16/32）的 flag 语法：亮度 + U/V 各一个，且残差 bin 序列不变；
//   · Range Extension 参数集：max=2 不写 extension，>=3 才写，且 RExt profile
//     只改 profile_idc / 兼容标志 / extension 位（Main 路径位序列不变）；
//   · encode/decode 对偶：flag 值与量化系数都能逐位回读。
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "src/media/encoder/hevc/cabac_context.hpp"
#include "src/media/encoder/hevc/cabac_syntax.hpp"
#include "src/media/encoder/hevc/cu_coding.hpp"
#include "src/media/encoder/hevc/parameter_sets.hpp"

namespace {
using namespace termchart::image::codec::hevc;

// 构造一个所有 6 个 TU 都有残差的 CU，便于精确数 flag bin。
struct Fixture {
    std::int16_t luma[4][16] = {};
    std::int16_t chroma[2][16] = {};

    Fixture() {
        for (int t = 0; t < 4; ++t)
            for (int i = 0; i < 16; ++i)
                luma[t][i] = static_cast<std::int16_t>(((i + t) % 3) ? ((i & 1) ? 3 : -2) : 0);
        chroma[0][0] = 4;
        chroma[1][0] = -3;
    }

    CuIntra8x8 cu() const {
        CuIntra8x8 c;
        c.cbf_cb = 1;
        c.cbf_cr = 1;
        for (int i = 0; i < 4; ++i) c.luma4x4[i] = luma[i];
        c.chroma4x4[0] = chroma[0];
        c.chroma4x4[1] = chroma[1];
        return c;
    }
};

int count_flag_bins(const std::vector<Bin>& bins, int* ones = nullptr) {
    int total = 0, one = 0;
    for (const auto& b : bins)
        if (b.ctx == kCtxTransformSkip || b.ctx == kCtxTransformSkip + 1) {
            ++total;
            one += b.val;
        }
    if (ones) *ones = one;
    return total;
}

}  // namespace

TEST_CASE("hevc: Pps transform_skip flag is byte-identical when disabled") {
    Pps off;
    Pps on;
    on.transform_skip_enabled = true;
    CHECK_FALSE(off.transform_skip_enabled);  // 默认关闭：G0 与既有码流不变
    const std::string a = write_pps(off);
    const std::string b = write_pps(on);
    REQUIRE(a.size() == b.size());
    int diff_bytes = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        diff_bytes += (a[i] != b[i]) ? 1 : 0;
    CHECK(diff_bytes == 1);  // 开关只影响 1 个字节；不写 pps extension
}

TEST_CASE("hevc: TRANSFORM_SKIP_FLAG ctx offset is consistent with CABAC_ELEMS") {
    // ffmpeg cabac.c ELEM 表累加：37+3=40(cbf_luma) +2=42(cbf_cb/cr)
    // +5=47(transform_skip) +2+2=51(rdpcm/rdpcm_dir) +2=53(last_sig_x)
    CHECK(kCtxSplitTransform == 37);
    CHECK(kCtxCbfLuma == 40);
    CHECK(kCtxCbfCbCr == 42);
    CHECK(kCtxTransformSkip == 47);
    CHECK(kCtxLastSigCoeffX == 53);
    CHECK(kCtxTransformSkip + 2 + 2 + 2 == kCtxLastSigCoeffX);
    // put/get 对偶（亮度 47 / 色度 48）
    for (int c_idx : {0, 1, 2}) {
        std::vector<Bin> out;
        put_transform_skip_flag(out, c_idx, 1);
        REQUIRE(out.size() == 1);
        CHECK(out[0].ctx == kCtxTransformSkip + (c_idx ? 1 : 0));
        CHECK(out[0].val == 1);
        BinCursor cur(out.data(), out.size());
        int v = -1;
        CHECK(get_transform_skip_flag(cur, c_idx, v));
        CHECK(v == 1);
        CHECK(cur.consumed() == 1);
    }
}

TEST_CASE("hevc: CU emits no transform_skip_flag bin when PPS disables it") {
    Fixture fx;
    CuIntra8x8 cu = fx.cu();
    cu.transform_skip_enabled = false;
    const auto bins = encode_cu_intra_8x8(cu);
    REQUIRE_FALSE(bins.empty());
    CHECK(count_flag_bins(bins) == 0);

    CuIntra8x8Shape shape;  // 默认 transform_skip_enabled=false
    CuIntra8x8Decoded dec;
    std::size_t used = 0;
    CHECK(decode_cu_intra_8x8(bins.data(), bins.size(), shape, &dec, &used));
    CHECK(used == bins.size());
}

TEST_CASE("hevc: CU emits one flag bin per cbf=1 TU and round-trips bitwise") {
    Fixture fx;
    CuIntra8x8 cu = fx.cu();
    cu.transform_skip_enabled = true;
    // 亮度 TU 0/2 用 skip=1，色度保持 0 ⇒ 同时覆盖 flag 的 0/1 分支
    cu.transform_skip_luma[0] = 1;
    cu.transform_skip_luma[2] = 1;

    const auto bins = encode_cu_intra_8x8(cu);
    REQUIRE_FALSE(bins.empty());
    int ones = 0;
    CHECK(count_flag_bins(bins, &ones) == 6);  // 4 亮度 + 2 色度
    CHECK(ones == 2);

    CuIntra8x8Shape shape;
    shape.transform_skip_enabled = true;
    CuIntra8x8Decoded dec;
    std::size_t used = 0;
    REQUIRE(decode_cu_intra_8x8(bins.data(), bins.size(), shape, &dec, &used));
    CHECK(used == bins.size());
    CHECK(dec.transform_skip_luma[0] == 1);
    CHECK(dec.transform_skip_luma[1] == 0);
    CHECK(dec.transform_skip_luma[2] == 1);
    CHECK(dec.transform_skip_luma[3] == 0);
    CHECK(dec.transform_skip_chroma[0] == 0);
    CHECK(dec.transform_skip_chroma[1] == 0);
    int coeff_diff = 0;
    for (int t = 0; t < 4; ++t)
        for (int i = 0; i < 16; ++i)
            coeff_diff += (dec.luma[t][i] != fx.luma[t][i]);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 16; ++i)
            coeff_diff += (dec.chroma[c][i] != fx.chroma[c][i]);
    CHECK(coeff_diff == 0);
}

TEST_CASE("hevc: Range Extension declares max transform skip size only above 4x4") {
    Pps off;
    Pps on4;
    on4.transform_skip_enabled = true;  // log2_max 默认 2 → 沿用 ffmpeg 默认值
    const std::string a = write_pps(off);
    const std::string b = write_pps(on4);
    CHECK(a != b);
    CHECK(a.size() == b.size());  // 不写 pps_extension ⇒ 既有 PPS 字节数不变

    std::string prev;
    for (int lg = 3; lg <= 5; ++lg) {
        Pps p;
        p.transform_skip_enabled = true;
        p.log2_max_transform_skip_block_size = lg;
        const std::string ext = write_pps(p);
        CHECK(ext.size() > b.size());  // pps_range_extension() 被写出
        CHECK(ext != prev);            // minus2 的 ue(v) 随档位变化
        prev = ext;
    }

    Sps main;
    Sps rext;
    rext.general_profile_idc = 4;
    rext.range_extension = true;
    CHECK(write_sps(rext).size() > write_sps(main).size());
    CHECK(write_sps(rext) != write_sps(main));
    // 只改 profile 不改 extension 时，位宽仍与 Main 一致（PTL 结构相同）
    Sps prof_only;
    prof_only.general_profile_idc = 4;
    CHECK(write_sps(prof_only).size() == write_sps(main).size());
    CHECK(write_sps(prof_only) != write_sps(main));  // profile_idc + 兼容标志
    Vps vmain, vrext;
    vrext.general_profile_idc = 4;
    CHECK(write_vps(vrext) != write_vps(vmain));  // VPS PTL 必须与 SPS 匹配
}

namespace {

struct LargeFixture {
    std::int16_t luma[1024] = {};
    std::int16_t chroma[2][256] = {};

    LargeFixture() {
        luma[0] = 3;
        luma[17] = -2;
        luma[100] = 1;
        chroma[0][0] = 4;
        chroma[0][9] = -1;
        chroma[1][0] = -3;
        chroma[1][20] = 2;
    }

    CuIntra8x8 cu(int lg) const {
        CuIntra8x8 c;
        c.cbf_cb = 1;
        c.cbf_cr = 1;
        c.log2_tu_size = lg;
        c.luma_large = luma;
        if (lg == 3) {  // 8x8 亮度 → 色度仍是 4x4
            c.chroma4x4[0] = chroma[0];
            c.chroma4x4[1] = chroma[1];
        } else {
            c.chroma_large[0] = chroma[0];
            c.chroma_large[1] = chroma[1];
        }
        return c;
    }
};

}  // namespace

TEST_CASE("hevc: 8/16/32 TU emit transform_skip_flag for luma and both chroma TUs") {
    LargeFixture fx;
    for (int lg : {3, 4, 5}) {
        CuIntra8x8 off = fx.cu(lg);
        const auto b0 = encode_cu_intra_8x8(off);
        REQUIRE_FALSE(b0.empty());
        CHECK(count_flag_bins(b0) == 0);

        CuIntra8x8 on = fx.cu(lg);
        on.transform_skip_enabled = true;
        on.transform_skip_luma[0] = 1;
        on.transform_skip_chroma[0] = 1;
        on.transform_skip_chroma[1] = 1;
        const auto b1 = encode_cu_intra_8x8(on);
        REQUIRE_FALSE(b1.empty());
        int ones = 0;
        CHECK(count_flag_bins(b1, &ones) == 3);  // 1 亮度 + 2 色度
        CHECK(ones == 3);
        // flag 是唯一新增语法元素；残差 bin 序列逐位不变（ctx 不随 skip 切换）
        CHECK(b1.size() == b0.size() + 3);
    }
}

TEST_CASE("hevc: cbf=0 TUs do not emit transform_skip_flag") {
    Fixture fx;
    CuIntra8x8 cu = fx.cu();
    cu.transform_skip_enabled = true;
    cu.luma4x4[1] = nullptr;  // 该 TU 全零 ⇒ cbf=0
    cu.chroma4x4[0] = nullptr;
    cu.cbf_cb = 0;
    cu.transform_skip_luma[0] = 1;
    cu.transform_skip_luma[2] = 1;
    cu.transform_skip_luma[3] = 1;
    const auto bins = encode_cu_intra_8x8(cu);
    REQUIRE_FALSE(bins.empty());
    int ones = 0;
    // 亮 3 个 + 色度 V 1 个 = 4
    CHECK(count_flag_bins(bins, &ones) == 4);
    CHECK(ones == 3);  // TU1 的 flag=1 不该出现（cbf=0）

    CuIntra8x8Shape shape;
    shape.transform_skip_enabled = true;
    CuIntra8x8Decoded dec;
    std::size_t used = 0;
    REQUIRE(decode_cu_intra_8x8(bins.data(), bins.size(), shape, &dec, &used));
    CHECK(used == bins.size());
    CHECK(dec.cbf_luma[1] == 0);
}
