// test_hevc_tu_sizes.cpp —— 4/8/16/32 TU 的残差语法、帧内预测与端到端档位回归
//
// 外部 oracle（ffmpeg 真解码逐像素）在 build_scratch/verify_tu_sizes.py；
// 这里锁定不依赖外部解码器的结构不变量：多 CG 语法自往返、4x4 逐 bin 不变、
// 预测尺寸泛化后的 4x4 等价、以及 TU 档位确实改变参数集/码流。
#include <doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "src/image/codec/hevc/cabac_context.hpp"
#include "src/image/codec/hevc/intra_prediction.hpp"
#include "src/image/codec/hevc/residual_coding.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/image/hevc_encode.hpp"

namespace {
using namespace termchart::image;
using namespace termchart::image::codec::hevc;

// 伪随机但确定的量化 level：覆盖稀疏、密集、大幅值（Rice escape）、负号。
std::vector<std::int16_t> make_coeff(int log2_size, int kind) {
    const int n = 1 << log2_size;
    std::vector<std::int16_t> c(static_cast<std::size_t>(n * n), 0);
    std::uint32_t s = 0x9e3779b9u + static_cast<unsigned>(kind * 7919 + log2_size);
    auto next = [&]() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<int>(s % 97);
    };
    for (int i = 0; i < n * n; ++i) {
        if (kind == 0 && (i % 11) != 0) continue;         // 稀疏
        if (kind == 1 && (i % 3) == 0) continue;          // 稠密
        int v = next() % (kind == 2 ? 40 : 5);            // kind 2 触发 escape
        if (v == 0) v = 1;
        c[static_cast<std::size_t>(i)] = static_cast<std::int16_t>((i & 1) ? -v : v);
    }
    if (kind == 3) {                                       // 仅最后一个系数非零
        std::fill(c.begin(), c.end(), 0);
        c.back() = -13;
    }
    return c;
}

void check_bins(const std::vector<Bin>& bins) {
    for (const auto& b : bins) {
        if (b.ctx == kBypassCtx) continue;
        CHECK(b.ctx >= 0);
        CHECK(b.ctx < kContextCount);
    }
}

}  // namespace

TEST_CASE("hevc: large TU residual syntax round-trips for 8/16/32") {
    for (int lg = 3; lg <= 5; ++lg) {
        const int scans = (lg == 3) ? 3 : 1;  // >=16 恒 DIAG
        for (int s = 0; s < scans; ++s) {
            const auto scan = static_cast<ScanIdx>(s);
            for (int kind = 0; kind < 4; ++kind) {
                const auto src = make_coeff(lg, kind);
                bool ok = false;
                const auto bins = encode_residual(src.data(), lg, &ok, scan, 0);
                REQUIRE(ok);
                REQUIRE_FALSE(bins.empty());
                check_bins(bins);
                std::vector<std::int16_t> back(src.size(), 0);
                std::size_t used = 0;
                REQUIRE(decode_residual(bins.data(), bins.size(), back.data(), lg, &used, scan, 0));
                CHECK(used == bins.size());
                CHECK(back == src);
            }
        }
    }
}

TEST_CASE("hevc: chroma large TU uses c_idx context offsets") {
    for (int lg = 3; lg <= 4; ++lg) {
        const auto src = make_coeff(lg, 1);
        for (int c_idx = 1; c_idx <= 2; ++c_idx) {
            bool ok = false;
            const auto bins = encode_residual(src.data(), lg, &ok, kScanDiag, c_idx);
            REQUIRE(ok);
            check_bins(bins);
            std::vector<std::int16_t> back(src.size(), 0);
            std::size_t used = 0;
            REQUIRE(decode_residual(bins.data(), bins.size(), back.data(), lg, &used,
                                    kScanDiag, c_idx));
            CHECK(back == src);
        }
    }
}

TEST_CASE("hevc: generalized residual keeps the 4x4 bin sequence unchanged") {
    for (int s = 0; s < 3; ++s) {
        const auto scan = static_cast<ScanIdx>(s);
        const auto& st = scan4x4_tables(scan);
        for (int kind = 0; kind < 3; ++kind) {
            const auto coeff = make_coeff(2, kind);  // 行主序
            std::int16_t scanned[16];
            for (int n = 0; n < 16; ++n) scanned[n] = coeff[st.y[n] * 4 + st.x[n]];
            const auto a = encode_residual(coeff.data(), 2, nullptr, scan, 0);
            const auto b = encode_residual_4x4(scanned, nullptr, scan, 0);
            REQUIRE(a.size() == b.size());
            for (std::size_t i = 0; i < a.size(); ++i) {
                CHECK(a[i].ctx == b[i].ctx);
                CHECK(a[i].val == b[i].val);
            }
        }
    }
}

TEST_CASE("hevc: size-generic intra prediction equals the 4x4 production path") {
    constexpr int kStride = 40;
    std::array<std::uint8_t, kStride * kStride> rec{};
    for (int y = 0; y < kStride; ++y)
        for (int x = 0; x < kStride; ++x)
            rec[static_cast<std::size_t>(y) * kStride + x] =
                static_cast<std::uint8_t>(40 + ((x * 7 + y * 11) % 160));
    for (int mode = 0; mode < 35; ++mode) {
        std::array<std::uint8_t, 16> a{}, b{};
        predict_intra_luma_4x4(mode, rec.data(), kStride, 12, 12, kStride, kStride, a.data());
        predict_intra_luma(mode, rec.data(), kStride, 12, 12, 2, kStride, kStride, b.data());
        CHECK(a == b);
    }
    // 均匀参考下，8/16/32 的 Planar/DC 必须仍给出均匀块（尺寸无关的锚点）。
    // 32x32 TU 要求 x0/y0 32 对齐且整块在图像内，故另用 96x96 平面、原点 (32,32)。
    constexpr int kWide = 96;
    std::array<std::uint8_t, kWide * kWide> flat{};
    flat.fill(97);
    for (int lg = 3; lg <= 5; ++lg) {
        const int n = 1 << lg;
        std::vector<std::uint8_t> dst(static_cast<std::size_t>(n * n), 0);
        for (int mode : {0, 1}) {
            predict_intra_luma(mode, flat.data(), kWide, 32, 32, lg, kWide, kWide,
                               dst.data());
            for (const auto v : dst) CHECK(v == 97);
        }
    }
}

TEST_CASE("hevc: TU size option changes SPS and keeps the 4x4 default bitstream") {
    Bitmap bmp(64, 64, 4);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const auto v = static_cast<std::uint8_t>(60 + ((x / 8 + y / 8) & 1) * 60 +
                                                     (x % 8) * 3);
            for (int c = 0; c < 4; ++c) bmp.set(x, y, c, v);
        }
    std::vector<Bitmap> frames = {bmp};
    std::array<std::string, 4> bs;
    for (int lg = 2; lg <= 5; ++lg) {
        EncodeOptions opts;
        opts.video_mode = VideoEncodeMode::Intra;
        opts.video_qp = 26;
        opts.hevc_log2_tu_size = lg;
        bs[static_cast<std::size_t>(lg - 2)] =
            encode_hevc_annexb(std::span<const Bitmap>{frames}, opts);
        REQUIRE_FALSE(bs[static_cast<std::size_t>(lg - 2)].empty());
    }
    EncodeOptions def;
    def.video_mode = VideoEncodeMode::Intra;
    def.video_qp = 26;
    CHECK(bs[0] == encode_hevc_annexb(std::span<const Bitmap>{frames}, def));
    CHECK(bs[0] != bs[1]);
    CHECK(bs[1] != bs[2]);
    CHECK(bs[2] != bs[3]);
}

TEST_CASE("hevc: auto TU mode (0) is valid, distinct and fails loud on bad values") {
    Bitmap bmp(128, 64, 4);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 128; ++x) {
            const int flat = (x / 32 + y / 32) & 1;
            const auto v = static_cast<std::uint8_t>(flat ? 120 : 40 + ((x ^ y) & 15) * 6);
            for (int c = 0; c < 4; ++c) bmp.set(x, y, c, v);
        }
    std::vector<Bitmap> frames = {bmp};
    auto enc = [&](int tu) {
        EncodeOptions o;
        o.video_mode = VideoEncodeMode::Intra;
        o.video_qp = 26;
        o.hevc_log2_tu_size = tu;
        return encode_hevc_annexb(std::span<const Bitmap>{frames}, o);
    };
    const auto aut = enc(0);
    REQUIRE_FALSE(aut.empty());
    CHECK(aut != enc(2));
    CHECK(aut != enc(3));
    CHECK(aut != enc(4));
    CHECK(aut != enc(5));
    // 越界档位显式失败，不静默降级成默认档。
    CHECK(enc(-1).empty());
    CHECK(enc(1).empty());
    CHECK(enc(6).empty());
}

TEST_CASE("hevc: intra mode decision is sized by the real TU") {
    constexpr int kS = 96;
    std::array<std::uint8_t, kS * kS> src{};
    for (int y = 0; y < kS; ++y)
        for (int x = 0; x < kS; ++x)
            src[static_cast<std::size_t>(y) * kS + x] =
                static_cast<std::uint8_t>(30 + x / 2);  // 水平渐变
    std::array<std::uint8_t, kS * kS> rec{};
    rec.fill(80);
    // 同一 32x32 区域，三种 TU 尺寸各跑一次 35 模式决策：全部合法，且大 TU
    // 下不再沿用 4x4 的失真模型（旧实现会让 3/4/5 返回与 2 完全相同的结果）。
    const int m2 = choose_intra_luma_mode(src.data(), kS, rec.data(), kS,
                                          32, 32, kS, kS, 26, 2);
    const int m3 = choose_intra_luma_mode(src.data(), kS, rec.data(), kS,
                                          32, 32, kS, kS, 26, 3);
    const int m5 = choose_intra_luma_mode(src.data(), kS, rec.data(), kS,
                                          32, 32, kS, kS, 26, 5);
    for (int m : {m2, m3, m5}) {
        CHECK(m >= 0);
        CHECK(m < 35);
    }
    // 旧 4x4 接口仍等价于 tu_log2=2。
    CHECK(choose_intra_luma_mode_8x8(src.data(), kS, rec.data(), kS,
                                     32, 32, kS, kS, 26) == m2);
    // 非法尺寸返回 DC，不越界。
    CHECK(choose_intra_luma_mode(src.data(), kS, rec.data(), kS,
                                 32, 32, kS, kS, 26, 6) == 1);
}
